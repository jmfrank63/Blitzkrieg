//! The editor's process: one SDL window, the engine started on it through
//! the bridge, and ImGui drawing into the engine's own frame. Nothing here
//! edits; the view and the panels (Tasks 4-5) sit on top.
const std = @import("std");
const builtin = @import("builtin");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
/// bridge.h, translated once for the app (c_bridge.zig).
pub const c = @import("c_bridge.zig").c;

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

    /// On failure, `failureReason()` says why: the bridge's own message when
    /// the bridge refused, SDL's when SDL did.
    pub fn start(options: Options) HostError!Host {
        failure_len = 0;
        if (!sdl3.c.SDL_Init(sdl3.c.SDL_INIT_VIDEO)) return failWith(error.SdlInitFailed, sdlError());
        errdefer sdl3.c.SDL_Quit();
        freeCommandW();
        // No SDL_WINDOW_HIGH_PIXEL_DENSITY: a point is a pixel, so a mouse
        // position is a screen position (see the plan's Decisions).
        var flags: sdl3.c.SDL_WindowFlags = sdl3.c.SDL_WINDOW_RESIZABLE;
        if (options.hidden) flags |= sdl3.c.SDL_WINDOW_HIDDEN;
        const window = sdl3.c.SDL_CreateWindow(options.title, options.width, options.height, flags) orelse
            return failWith(error.WindowFailed, sdlError());
        errdefer sdl3.c.SDL_DestroyWindow(window);

        var session: ?*c.BkEditorSession = null;
        const started = c.BkEditorStart(window, options.data_root, &session);
        if (started != c.BK_EDITOR_OK) {
            // Kept before BkEditorStop, which may replace the message.
            keepReason(std.mem.span(c.BkEditorLastMessage(session)));
            std.debug.print("map-editor: the engine did not start: {s}\n", .{failureReason()});
            if (session) |s| _ = c.BkEditorStop(s);
            return if (started == c.BK_EDITOR_NO_DEVICE) error.NoDevice else error.EngineFailed;
        }
        errdefer _ = c.BkEditorStop(session.?);

        var device: ?*anyopaque = null;
        var format: c_uint = 0;
        if (c.BkEditorGpuDevice(session, &device, &format) != c.BK_EDITOR_OK)
            return failWith(error.NoDevice, std.mem.span(c.BkEditorLastMessage(session)));
        _ = imgui.c.igCreateContext(null);
        errdefer imgui.c.igDestroyContext(null);
        imgui.c.igGetIO().*.IniFilename = null;
        if (!imgui.c.bk_imgui_backend_init(@ptrCast(window), device, format))
            return failWith(error.ImguiFailed, "the ImGui SDL3/SDL_GPU backend did not initialise");
        errdefer imgui.c.bk_imgui_backend_shutdown();
        if (c.BkEditorSetOverlay(session, overlay, null) != c.BK_EDITOR_OK)
            return failWith(error.ImguiFailed, std.mem.span(c.BkEditorLastMessage(session)));
        return .{ .window = window, .session = session.? };
    }

    /// Takes the overlay and ImGui down, stops the session (BkEditorStop
    /// deletes the world and removes the overlay) and destroys the window.
    /// The engine's renderer and its GPU device are not shut down: they live
    /// until the process exits. One Host per process is the contract - a
    /// second start after a stop is not supported.
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

/// Why the last Host.start failed, copied out of the bridge or SDL before
/// either is shut down, so a dialog can show it after the failure has
/// unwound. One Host per process, so one reason.
var failure_buffer: [512]u8 = undefined;
var failure_len: usize = 0;

/// The reason the last Host.start failed, or "" when it did not fail.
pub fn failureReason() []const u8 {
    return failure_buffer[0..failure_len];
}

fn keepReason(reason: []const u8) void {
    failure_len = @min(reason.len, failure_buffer.len);
    @memcpy(failure_buffer[0..failure_len], reason[0..failure_len]);
}

fn failWith(err: HostError, reason: []const u8) HostError {
    keepReason(reason);
    return err;
}

fn sdlError() []const u8 {
    return std.mem.span(sdl3.c.SDL_GetError());
}

/// SDL's macOS menu bar has Window > Close on Cmd+W: AppKit takes the key
/// before SDL sees it and closes the window, which quits the editor. Cmd+W
/// is File > Close (the map), so the item loses its key equivalent and the
/// key reaches the editor. The red close button still quits. The menus exist
/// once SDL_Init has started the video subsystem (Cocoa_RegisterApp).
/// libobjc is AppKit's, loaded by SDL, so it is looked up rather than linked.
/// `command_w_freed` says whether it worked (the host check requires it).
fn freeCommandW() void {
    if (builtin.os.tag != .macos) return;
    const Id = ?*anyopaque;
    const default_handle: ?*anyopaque = @ptrFromInt(@as(usize, @bitCast(@as(isize, -2)))); // RTLD_DEFAULT
    const get_class: *const fn ([*:0]const u8) callconv(.c) Id = @ptrCast(@alignCast(std.c.dlsym(default_handle, "objc_getClass") orelse return));
    const register_name: *const fn ([*:0]const u8) callconv(.c) Id = @ptrCast(@alignCast(std.c.dlsym(default_handle, "sel_registerName") orelse return));
    const msg_send = std.c.dlsym(default_handle, "objc_msgSend") orelse return;
    const send0: *const fn (Id, Id) callconv(.c) Id = @ptrCast(@alignCast(msg_send));
    const send_id: *const fn (Id, Id, Id) callconv(.c) Id = @ptrCast(@alignCast(msg_send));
    const send_str: *const fn (Id, Id, [*:0]const u8) callconv(.c) Id = @ptrCast(@alignCast(msg_send));

    const string_class = get_class("NSString") orelse return;
    const app = send0(get_class("NSApplication"), register_name("sharedApplication")) orelse return;
    const window_menu = send0(app, register_name("windowsMenu")) orelse return;
    const close_title = send_str(string_class, register_name("stringWithUTF8String:"), "Close") orelse return;
    const close_item = send_id(window_menu, register_name("itemWithTitle:"), close_title) orelse return;
    const empty = send_str(string_class, register_name("stringWithUTF8String:"), "") orelse return;
    _ = send_id(close_item, register_name("setKeyEquivalent:"), empty);
    const key = send0(close_item, register_name("keyEquivalent")) orelse return;
    const length: *const fn (Id, Id) callconv(.c) usize = @ptrCast(@alignCast(msg_send));
    command_w_freed = length(key, register_name("length")) == 0;
}

/// True once Window > Close no longer owns Cmd+W (macOS; `freeCommandW`).
pub var command_w_freed = false;

fn overlay(user: ?*anyopaque, command_buffer: ?*anyopaque, target: ?*anyopaque, width: c_uint, height: c_uint) callconv(.c) void {
    _ = user;
    _ = width;
    _ = height;
    imgui.c.bk_imgui_backend_render(command_buffer, target);
}
