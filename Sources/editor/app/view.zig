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
/// until the object palette chooses another. Also main.zig's --game-reads-it
/// mode, which places the catalogue's first unit the same way the placer's
/// default does.
pub const unit_game_type: i32 = 1;

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
    pan_anchor: tools.Pointer = .{ .world_x = 0, .world_y = 0, .map_x = 0, .map_y = 0 },
    /// True between a left press the view saw and its release: an open
    /// tool gesture, which keeps the view routing motion and the release
    /// even if the cursor strays over an ImGui panel mid-drag.
    left_button_down: bool = false,
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

    /// The view's own part of the status bar: "failed: " and the reason for
    /// anything worse than a refusal - a tool's or a panel's failed edit, or
    /// a lost frame main.zig reports. A refusal is not repeated here; the
    /// editor's own status already words it. Empty after the next edit that
    /// succeeds or is merely refused, so a failure never outlives the edits
    /// after it.
    pub fn statusLine(self: *const View) []const u8 {
        return self.status_buffer[0..self.status_len];
    }

    /// True while a mouse gesture the view started (a left-button tool
    /// gesture, or a middle-button camera pan) is still open. main.zig's
    /// event loop uses this so a gesture that started on the map keeps
    /// reaching the view even if the cursor drifts over an ImGui panel
    /// before it ends.
    pub fn hasActiveMouseGesture(self: *const View) bool {
        return self.left_button_down or self.panning;
    }

    /// `error.Refused` is not an error here: the editor's status line
    /// already holds the reason. Anything else is shown too, prefixed
    /// "failed:". Exposed for main.zig to report failures view.zig itself
    /// did not cause (a lost GPU frame, say).
    pub fn setStatus(self: *View, prefix: []const u8, message: []const u8) void {
        const prefix_len = @min(prefix.len, self.status_buffer.len);
        @memcpy(self.status_buffer[0..prefix_len], prefix[0..prefix_len]);
        const message_len = @min(message.len, self.status_buffer.len - prefix_len);
        @memcpy(self.status_buffer[prefix_len..][0..message_len], message[0..message_len]);
        self.status_len = prefix_len + message_len;
    }

    pub fn clearStatus(self: *View) void {
        self.status_len = 0;
    }

    /// The status of an edit made anywhere - a tool, a menu, a panel -
    /// onto the view's part of the status bar (see `statusLine`).
    pub fn noteEditResult(self: *View, editor: *Editor, result: EditError!void) void {
        result catch |err| return self.noteToolError(editor, err);
        self.clearStatus();
    }

    /// The tool palette's and the menu's way to switch tools: the same as
    /// the 1/2/3 keys, an open gesture on the old tool ended first.
    pub fn selectTool(self: *View, editor: *Editor, tool: Tool) void {
        self.switchTool(editor, tool);
    }

    /// The object palette's choice: the placer's object from now on. The
    /// name is copied, so the caller's buffer may go.
    pub fn setPlacerObject(self: *View, name: []const u8) void {
        const len = @min(name.len, self.placer_name_storage.len);
        @memcpy(self.placer_name_storage[0..len], name[0..len]);
        self.placer.name = self.placer_name_storage[0..len];
    }

    /// Undo and redo for the Edit menu, reported as the keys report them.
    pub fn undo(self: *View, editor: *Editor) void {
        self.runUndoable(editor, .undo);
    }

    pub fn redo(self: *View, editor: *Editor) void {
        self.runUndoable(editor, .redo);
    }

    /// Centres the camera on a freshly opened map, and picks the placer's
    /// object if none was chosen yet: the first catalogue entry of game type
    /// unit. Called after every `editor.open` that succeeds.
    pub fn centreOn(self: *View, real: *RealBridge, info: MapInfo) void {
        self.map = .{ .width_tiles = info.width_tiles, .height_tiles = info.height_tiles };
        self.camera_x = @as(f32, @floatFromInt(info.width_tiles)) * world_cell_size / 2;
        self.camera_y = @as(f32, @floatFromInt(info.height_tiles)) * world_cell_size / 2;
        _ = real.setCamera(self.camera_x, self.camera_y);
        if (self.placer.name.len == 0) self.pickDefaultPlacerObject(real);
    }

    fn pickDefaultPlacerObject(self: *View, real: *RealBridge) void {
        const entries = real.catalogue(self.allocator) catch return;
        defer self.allocator.free(entries);
        for (entries) |entry| {
            if (entry.game_type != unit_game_type) continue;
            self.setPlacerObject(std.mem.sliceTo(&entry.name, 0));
            return;
        }
    }

    /// An event `main.zig` decided (through `inputKindOf`/`shouldDeliver`)
    /// belongs to the view. Any edit's outcome goes through
    /// `noteEditResult`: a refusal is left to the editor's own status, a
    /// failure is prefixed "failed:".
    pub fn handleEvent(self: *View, editor: *Editor, real: *RealBridge, event: *const sdl3.c.SDL_Event) void {
        switch (event.type) {
            sdl3.c.SDL_EVENT_MOUSE_BUTTON_DOWN, sdl3.c.SDL_EVENT_MOUSE_BUTTON_UP => {
                const button = event.button;
                if (button.button == sdl3.c.SDL_BUTTON_MIDDLE) {
                    self.handleMiddleButton(editor, button);
                    return;
                }
                const kind = view_math.kindOf(.{ .button = button.button, .down = button.down }) orelse return;
                switch (kind) {
                    .press => {
                        // A press that cannot resolve starts no gesture: the
                        // tool has nothing to press on.
                        const pointer = editor.resolve(button.x, button.y) catch return;
                        self.hover = pointer;
                        self.left_button_down = true;
                        self.dispatch(editor, .{ .press = pointer });
                    },
                    .release => {
                        // Unlike a press, a release must still reach the
                        // tool even when the cursor has drifted off the
                        // terrain (or over a panel, which is why this event
                        // was routed here at all): the last known point
                        // ends the gesture cleanly rather than leaving it
                        // open for a later press to merge into.
                        const pointer = editor.resolve(button.x, button.y) catch (self.hover orelse tools.Pointer{ .world_x = 0, .world_y = 0, .map_x = 0, .map_y = 0 });
                        self.hover = pointer;
                        self.left_button_down = false;
                        self.dispatch(editor, .{ .release = pointer });
                    },
                }
            },
            sdl3.c.SDL_EVENT_MOUSE_MOTION => self.handleMotion(editor, real, event.motion),
            sdl3.c.SDL_EVENT_MOUSE_WHEEL => self.handleWheel(real, event.wheel),
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

    /// A mouse wheel or a two-finger trackpad swipe pans the camera. SDL
    /// sends a swipe as many small fractional wheel events on both axes.
    /// view_math.wheelPan maps each straight to a screen pan, so the camera
    /// follows the fingers without rounding or stepping back.
    fn handleWheel(self: *View, real: *RealBridge, wheel: sdl3.c.SDL_MouseWheelEvent) void {
        const pan = view_math.wheelPan(.{ .x = wheel.x, .y = wheel.y, .flipped = wheel.direction == sdl3.c.SDL_MOUSEWHEEL_FLIPPED }, view_math.wheel_sensitivity);
        const before_x = self.camera_x;
        const before_y = self.camera_y;
        var camera: view_math.Camera = .{ .x = self.camera_x, .y = self.camera_y };
        camera.panScreen(pan.right_px, pan.up_px, self.map);
        self.camera_x = camera.x;
        self.camera_y = camera.y;
        if (self.camera_x != before_x or self.camera_y != before_y) _ = real.setCamera(self.camera_x, self.camera_y);
    }

    /// Q/E and the 1/2/3 tool keys ignore SDL's key-repeat (rotating by 16
    /// steps or hopping through tools because a key was held would surprise
    /// more than it would help). Delete/Backspace and undo/redo keep repeat:
    /// holding Cmd/Ctrl+Z to walk back several edits, or Delete to keep
    /// pressing it while nothing is selected, are both ordinary editor
    /// habits, and a delete or undo that repeats onto nothing just answers
    /// `error.Refused` harmlessly.
    fn handleKey(self: *View, editor: *Editor, key: sdl3.c.SDL_KeyboardEvent) void {
        const command_or_control = key.mod & (sdl3.c.SDL_KMOD_CTRL | sdl3.c.SDL_KMOD_GUI) != 0;
        switch (key.key) {
            sdl3.c.SDLK_DELETE, sdl3.c.SDLK_BACKSPACE => self.dispatch(editor, .{ .key = .delete }),
            sdl3.c.SDLK_Q => if (!key.repeat) self.dispatch(editor, .{ .key = .rotate_left }),
            sdl3.c.SDLK_E => if (!key.repeat) self.dispatch(editor, .{ .key = .rotate_right }),
            sdl3.c.SDLK_1 => if (!key.repeat) self.switchTool(editor, .select),
            sdl3.c.SDLK_2 => if (!key.repeat) self.switchTool(editor, .brush),
            sdl3.c.SDLK_3 => if (!key.repeat) self.switchTool(editor, .place),
            sdl3.c.SDLK_Z => if (command_or_control) {
                if (key.mod & sdl3.c.SDL_KMOD_SHIFT != 0) self.runUndoable(editor, .redo) else self.runUndoable(editor, .undo);
            },
            sdl3.c.SDLK_Y => if (command_or_control) self.runUndoable(editor, .redo),
            else => {},
        }
    }

    /// Switches the active tool, ending an open left-button gesture on the
    /// old one first: without this, painting or dragging with the mouse
    /// still held while pressing 1/2/3 would leave the old tool's gesture
    /// open (`Brush.gesture`/`Selector.gesture` non-zero), and the next
    /// press on the new tool would merge into it as one undo step.
    fn switchTool(self: *View, editor: *Editor, tool: Tool) void {
        if (self.left_button_down) {
            const pointer = self.hover orelse tools.Pointer{ .world_x = 0, .world_y = 0, .map_x = 0, .map_y = 0 };
            self.dispatch(editor, .{ .release = pointer });
            self.left_button_down = false;
        }
        self.tool = tool;
    }

    const UndoableDirection = enum { undo, redo };

    fn runUndoable(self: *View, editor: *Editor, direction: UndoableDirection) void {
        const result = switch (direction) {
            .undo => editor.undo(),
            .redo => editor.redo(),
        };
        if (result) |_| self.clearStatus() else |err| self.noteToolError(editor, err);
    }

    fn dispatch(self: *View, editor: *Editor, event: tools.Event) void {
        const result = switch (self.tool) {
            .select => self.selector.handle(editor, event),
            .brush => self.brush.handle(editor, event),
            .place => self.placer.handle(editor, event),
        };
        self.noteEditResult(editor, result);
    }

    /// `error.Refused` is not an error here: the editor's status line
    /// already holds the reason, so the view's part is cleared. Anything
    /// else is shown, prefixed "failed:".
    fn noteToolError(self: *View, editor: *Editor, err: EditError) void {
        switch (err) {
            error.Refused => self.clearStatus(),
            error.OutOfMemory => self.setStatus("failed: ", "out of memory"),
            error.Failed => self.setStatus("failed: ", editor.status()),
        }
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
    /// view itself never saw a motion event this frame - but only while our
    /// own window has mouse focus and ImGui does not want the mouse, or the
    /// cursor sitting over another window, or a panel's edge, would scroll
    /// the map underneath it.
    pub fn update(self: *View, real: *RealBridge, window: *sdl3.c.SDL_Window, dt_seconds: f32) void {
        var dir: view_math.Scroll = .{};
        const capture = captureFlags();
        if (!capture.keyboard) {
            var count: c_int = 0;
            if (sdl3.c.SDL_GetKeyboardState(&count)) |keys| {
                if (keys[scancodeIndex(sdl3.c.SDL_SCANCODE_LEFT)] or keys[scancodeIndex(sdl3.c.SDL_SCANCODE_A)]) dir.left = true;
                if (keys[scancodeIndex(sdl3.c.SDL_SCANCODE_RIGHT)] or keys[scancodeIndex(sdl3.c.SDL_SCANCODE_D)]) dir.right = true;
                if (keys[scancodeIndex(sdl3.c.SDL_SCANCODE_UP)] or keys[scancodeIndex(sdl3.c.SDL_SCANCODE_W)]) dir.up = true;
                if (keys[scancodeIndex(sdl3.c.SDL_SCANCODE_DOWN)] or keys[scancodeIndex(sdl3.c.SDL_SCANCODE_S)]) dir.down = true;
            }
        }
        if (!capture.mouse and sdl3.c.SDL_GetMouseFocus() == window) {
            var mouse_x: f32 = -1;
            var mouse_y: f32 = -1;
            _ = sdl3.c.SDL_GetMouseState(&mouse_x, &mouse_y);
            if (real.screenSize()) |size| {
                const width: f32 = @floatFromInt(size[0]);
                const height: f32 = @floatFromInt(size[1]);
                if (mouse_x >= 0 and mouse_x <= edge_scroll_margin) dir.left = true;
                if (mouse_x < width and mouse_x >= width - edge_scroll_margin) dir.right = true;
                if (mouse_y >= 0 and mouse_y <= edge_scroll_margin) dir.up = true;
                if (mouse_y < height and mouse_y >= height - edge_scroll_margin) dir.down = true;
            }
        }
        if (!dir.left and !dir.right and !dir.up and !dir.down) return;
        const before_x = self.camera_x;
        const before_y = self.camera_y;
        self.scrollCamera(dir, dt_seconds);
        if (self.camera_x != before_x or self.camera_y != before_y) _ = real.setCamera(self.camera_x, self.camera_y);
    }
};

/// What kind of routing decision an SDL event needs (`view_math.shouldDeliver`):
/// coarser than `view_math.kindOf`, which only concerns the left mouse
/// button's press/release mapping.
/// Takes `SDL_Event.type` as it is, a Uint32: `SDL_EventType` is the C enum,
/// which translates to c_int on MSVC and c_uint elsewhere, so it cannot be
/// the parameter's type on both.
pub fn inputKindOf(event_type: @FieldType(sdl3.c.SDL_Event, "type")) view_math.InputEventKind {
    return switch (event_type) {
        sdl3.c.SDL_EVENT_MOUSE_BUTTON_DOWN, sdl3.c.SDL_EVENT_MOUSE_BUTTON_UP => .mouse_button,
        sdl3.c.SDL_EVENT_MOUSE_MOTION => .mouse_motion,
        sdl3.c.SDL_EVENT_MOUSE_WHEEL => .mouse_wheel,
        sdl3.c.SDL_EVENT_KEY_DOWN, sdl3.c.SDL_EVENT_KEY_UP => .key,
        else => .other,
    };
}

/// ImGui's capture flags, read after `host.handleEvent` has processed the
/// current event (its bool return is `ImGui_ImplSDL3_ProcessEvent`'s "I
/// processed this", not "I want this" - see `view_math.shouldDeliver`'s doc
/// comment for why routing cannot use it).
pub fn captureFlags() view_math.Capture {
    const io = imgui.c.igGetIO();
    return .{ .mouse = io.*.WantCaptureMouse, .keyboard = io.*.WantCaptureKeyboard };
}

/// SDL_Scancode's C enum values, as plain integers for indexing
/// SDL_GetKeyboardState's array.
fn scancodeIndex(value: anytype) usize {
    return @intCast(value);
}
