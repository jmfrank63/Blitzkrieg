//! The map view: the camera, and the mouse and keys that drive the core's
//! tools. Everything here that needs no window is in view_math.zig, tested
//! without the engine; this file wires it to SDL events, ImGui's capture
//! flags and the real bridge.
const std = @import("std");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const core = @import("editor_core");
const c_bridge = @import("c_bridge.zig");
const view_math = @import("view_math.zig");

const Editor = core.editor.Editor;
const MapInfo = core.bridge.MapInfo;
const EditError = core.bridge.EditError;
const tools = core.tools;
const RealBridge = c_bridge.RealBridge;

pub const world_cell_size = view_math.world_cell_size;

pub const Tool = enum { select, brush, place };

/// SGVOGT_UNIT (Sources/src/Main/GameDB.h): the placer's default object,
/// until the palette (a later task) lets the user choose one.
const unit_game_type: i32 = 1;

/// Screen pixels from a window edge that starts edge-scrolling.
const edge_scroll_margin: f32 = 8.0;

pub const View = struct {
    camera_x: f32 = 0,
    camera_y: f32 = 0,
    tool: Tool = .select,
    brush: tools.Brush,
    placer: tools.Placer,
    selector: tools.Selector = .{},
    hover: ?tools.Pointer = null,

    map: view_math.MapSize = .{},
    panning: bool = false,
    pan_anchor: tools.Pointer = .{ .world_x = 0, .world_y = 0 },
    placer_name_storage: [64]u8 = undefined,
    status_buffer: [512]u8 = undefined,
    status_len: usize = 0,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) View {
        return .{
            .allocator = allocator,
            .brush = .{ .tile = 0 },
            .placer = .{ .name = "" },
        };
    }

    pub fn deinit(self: *View, allocator: std.mem.Allocator) void {
        self.brush.deinit(allocator);
        self.* = undefined;
    }

    /// The status bar's text: the reason for the last refusal (as the
    /// editor already holds it), or "failed: " and the reason for anything
    /// worse. Empty once nothing has gone wrong yet.
    pub fn statusLine(self: *const View) []const u8 {
        return self.status_buffer[0..self.status_len];
    }

    /// Centres the camera on a freshly opened map, and picks the placer's
    /// object: the first catalogue entry of game type unit. Called once
    /// after `editor.open` succeeds.
    pub fn centreOn(self: *View, real: *RealBridge, info: MapInfo) void {
        self.map = .{ .width_tiles = info.width_tiles, .height_tiles = info.height_tiles };
        self.camera_x = @as(f32, @floatFromInt(info.width_tiles)) * world_cell_size / 2;
        self.camera_y = @as(f32, @floatFromInt(info.height_tiles)) * world_cell_size / 2;
        _ = real.setCamera(self.camera_x, self.camera_y);
        self.pickDefaultPlacerObject(real);
    }

    fn pickDefaultPlacerObject(self: *View, real: *RealBridge) void {
        const entries = real.catalogue(self.allocator) catch return;
        defer self.allocator.free(entries);
        for (entries) |entry| {
            if (entry.game_type != unit_game_type) continue;
            const name = std.mem.sliceTo(&entry.name, 0);
            const len = @min(name.len, self.placer_name_storage.len);
            @memcpy(self.placer_name_storage[0..len], name[0..len]);
            self.placer.name = self.placer_name_storage[0..len];
            return;
        }
    }

    /// An SDL event ImGui did not take. Any edit's error ends up on the
    /// status line: `error.Refused` as the editor already worded it,
    /// anything else prefixed "failed:".
    pub fn handleEvent(self: *View, editor: *Editor, real: *RealBridge, event: *const sdl3.c.SDL_Event) void {
        switch (event.type) {
            sdl3.c.SDL_EVENT_MOUSE_BUTTON_DOWN, sdl3.c.SDL_EVENT_MOUSE_BUTTON_UP => {
                const button = event.button;
                if (button.button == sdl3.c.SDL_BUTTON_MIDDLE) {
                    self.handleMiddleButton(editor, button);
                    return;
                }
                const kind = view_math.kindOf(.{ .button = button.button, .down = button.down }) orelse return;
                const pointer = editor.resolve(button.x, button.y) catch return;
                self.hover = pointer;
                self.dispatch(editor, switch (kind) {
                    .press => .{ .press = pointer },
                    .release => .{ .release = pointer },
                });
            },
            sdl3.c.SDL_EVENT_MOUSE_MOTION => self.handleMotion(editor, real, event.motion),
            sdl3.c.SDL_EVENT_KEY_DOWN => self.handleKey(editor, event.key),
            else => {},
        }
    }

    fn handleMiddleButton(self: *View, editor: *Editor, button: sdl3.c.SDL_MouseButtonEvent) void {
        if (button.down) {
            self.pan_anchor = editor.resolve(button.x, button.y) catch return;
            self.panning = true;
        } else {
            self.panning = false;
        }
    }

    fn handleMotion(self: *View, editor: *Editor, real: *RealBridge, motion: sdl3.c.SDL_MouseMotionEvent) void {
        if (self.panning) {
            const pointer = editor.resolve(motion.x, motion.y) catch return;
            // Moves the camera so the point grabbed at press stays under
            // the cursor - the same grab-offset idea as tools.Selector.
            self.panCamera(self.pan_anchor.world_x - pointer.world_x, self.pan_anchor.world_y - pointer.world_y);
            _ = real.setCamera(self.camera_x, self.camera_y);
            return;
        }
        const pointer = editor.resolve(motion.x, motion.y) catch {
            self.hover = null;
            return;
        };
        self.hover = pointer;
        if (motion.state & sdl3.c.SDL_BUTTON_LMASK != 0) self.dispatch(editor, .{ .drag = pointer });
    }

    fn handleKey(self: *View, editor: *Editor, key: sdl3.c.SDL_KeyboardEvent) void {
        const command_or_control = key.mod & (sdl3.c.SDL_KMOD_CTRL | sdl3.c.SDL_KMOD_GUI) != 0;
        switch (key.key) {
            sdl3.c.SDLK_DELETE, sdl3.c.SDLK_BACKSPACE => self.dispatch(editor, .{ .key = .delete }),
            sdl3.c.SDLK_Q => self.dispatch(editor, .{ .key = .rotate_left }),
            sdl3.c.SDLK_E => self.dispatch(editor, .{ .key = .rotate_right }),
            sdl3.c.SDLK_1 => self.tool = .select,
            sdl3.c.SDLK_2 => self.tool = .brush,
            sdl3.c.SDLK_3 => self.tool = .place,
            sdl3.c.SDLK_Z => if (command_or_control) {
                if (key.mod & sdl3.c.SDL_KMOD_SHIFT != 0) self.runUndoable(editor, .redo) else self.runUndoable(editor, .undo);
            },
            sdl3.c.SDLK_Y => if (command_or_control) self.runUndoable(editor, .redo),
            else => {},
        }
    }

    const UndoableDirection = enum { undo, redo };

    fn runUndoable(self: *View, editor: *Editor, direction: UndoableDirection) void {
        const result = switch (direction) {
            .undo => editor.undo(),
            .redo => editor.redo(),
        };
        _ = result catch |err| self.noteToolError(editor, err);
    }

    fn dispatch(self: *View, editor: *Editor, event: tools.Event) void {
        const result = switch (self.tool) {
            .select => self.selector.handle(editor, event),
            .brush => self.brush.handle(editor, event),
            .place => self.placer.handle(editor, event),
        };
        result catch |err| self.noteToolError(editor, err);
    }

    /// `error.Refused` is not an error here: the editor's status line
    /// already holds the reason. Anything else is shown too, prefixed
    /// "failed:".
    fn noteToolError(self: *View, editor: *Editor, err: EditError) void {
        switch (err) {
            error.Refused => self.setStatus("", editor.status()),
            error.OutOfMemory => self.setStatus("failed: ", "out of memory"),
            error.Failed => self.setStatus("failed: ", editor.status()),
        }
    }

    fn setStatus(self: *View, prefix: []const u8, message: []const u8) void {
        const prefix_len = @min(prefix.len, self.status_buffer.len);
        @memcpy(self.status_buffer[0..prefix_len], prefix[0..prefix_len]);
        const message_len = @min(message.len, self.status_buffer.len - prefix_len);
        @memcpy(self.status_buffer[prefix_len..][0..message_len], message[0..message_len]);
        self.status_len = prefix_len + message_len;
    }

    fn scrollCamera(self: *View, dir: view_math.Scroll, dt_seconds: f32) void {
        var camera: view_math.Camera = .{ .x = self.camera_x, .y = self.camera_y };
        camera.scroll(dir, dt_seconds, self.map);
        self.camera_x = camera.x;
        self.camera_y = camera.y;
    }

    fn panCamera(self: *View, delta_x: f32, delta_y: f32) void {
        var camera: view_math.Camera = .{ .x = self.camera_x + delta_x, .y = self.camera_y + delta_y };
        camera.clamp(self.map);
        self.camera_x = camera.x;
        self.camera_y = camera.y;
    }

    /// Per frame: keyboard and edge scrolling, then the camera. Keys only
    /// scroll when ImGui does not want the keyboard; edge-scrolling reads
    /// the mouse directly (SDL_GetMouseState), so it still works while the
    /// view itself never saw a motion event this frame.
    pub fn update(self: *View, real: *RealBridge, dt_seconds: f32) void {
        var dir: view_math.Scroll = .{};
        if (!imgui.c.igGetIO().*.WantCaptureKeyboard) {
            var count: c_int = 0;
            if (sdl3.c.SDL_GetKeyboardState(&count)) |keys| {
                if (keys[scancodeIndex(sdl3.c.SDL_SCANCODE_LEFT)] or keys[scancodeIndex(sdl3.c.SDL_SCANCODE_A)]) dir.left = true;
                if (keys[scancodeIndex(sdl3.c.SDL_SCANCODE_RIGHT)] or keys[scancodeIndex(sdl3.c.SDL_SCANCODE_D)]) dir.right = true;
                if (keys[scancodeIndex(sdl3.c.SDL_SCANCODE_UP)] or keys[scancodeIndex(sdl3.c.SDL_SCANCODE_W)]) dir.up = true;
                if (keys[scancodeIndex(sdl3.c.SDL_SCANCODE_DOWN)] or keys[scancodeIndex(sdl3.c.SDL_SCANCODE_S)]) dir.down = true;
            }
        }
        var mouse_x: f32 = -1;
        var mouse_y: f32 = -1;
        _ = sdl3.c.SDL_GetMouseState(&mouse_x, &mouse_y);
        if (real.screenSize()) |size| {
            const width: f32 = @floatFromInt(size[0]);
            const height: f32 = @floatFromInt(size[1]);
            if (mouse_x >= 0 and mouse_x <= edge_scroll_margin) dir.left = true;
            if (mouse_x <= width and mouse_x >= width - edge_scroll_margin) dir.right = true;
            if (mouse_y >= 0 and mouse_y <= edge_scroll_margin) dir.up = true;
            if (mouse_y <= height and mouse_y >= height - edge_scroll_margin) dir.down = true;
        }
        if (!dir.left and !dir.right and !dir.up and !dir.down) return;
        const before_x = self.camera_x;
        const before_y = self.camera_y;
        self.scrollCamera(dir, dt_seconds);
        if (self.camera_x != before_x or self.camera_y != before_y) _ = real.setCamera(self.camera_x, self.camera_y);
    }
};

/// SDL_Scancode's C enum values, as plain integers for indexing
/// SDL_GetKeyboardState's array.
fn scancodeIndex(value: anytype) usize {
    return @intCast(value);
}
