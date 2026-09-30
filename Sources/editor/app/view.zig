//! The map view: the camera, and the mouse and keys that drive the core's
//! tools. Everything here that needs no window is in view_math.zig, tested
//! without the engine; this file wires it to SDL events, ImGui's capture
//! flags and the real bridge. The wiring is tested too (the tests at the
//! end, `zig build test-map-editor-view`): `ViewWith` takes its input state
//! as a type and the bridge as `anytype`, so the tests feed real SDL events
//! through it against fakes, with no window, engine or ImGui.
const std = @import("std");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const core = @import("editor_core");
const view_math = @import("view_math.zig");
const tool_registry = @import("tool_registry.zig");

const Editor = core.editor.Editor;
const MapInfo = core.bridge.MapInfo;
const EditError = core.bridge.EditError;
const tools = core.tools;

pub const world_cell_size = view_math.world_cell_size;

/// The active tool's id; what each tool takes as input (the right button,
/// double click, Ctrl-as-right) and its shortcut are in tool_registry.zig.
pub const Tool = tool_registry.ToolId;

/// A map's camera and zoom, kept for the session only (D-15): not written
/// anywhere, forgotten when the editor quits.
pub const SavedView = struct { camera_x: f32, camera_y: f32, zoom_steps: i32 };

/// SGVOGT_UNIT (Sources/src/Main/GameDB.h): the placer's default object,
/// until the object palette chooses another. Also main.zig's --game-reads-it
/// mode, which places the catalogue's first unit the same way the placer's
/// default does.
pub const unit_game_type: i32 = 1;

/// Screen pixels from a window edge that starts edge-scrolling.
const edge_scroll_margin: f32 = 8.0;

/// What the view reads of the input devices' current state, beyond the
/// event in hand: the modifier keys (Shift+wheel zooms), the mouse's
/// position and buttons (a pinch zooms at the pointer; a stale gesture is
/// one whose button is no longer down), the keys held (arrows/WASD scroll),
/// whether the mouse is over our window, and ImGui's capture flags. The app
/// reads them from SDL and ImGui; view.zig's own tests from a fake, which
/// is what lets them run with no window, no linked SDL and no ImGui.
pub const SdlInput = struct {
    pub const Window = *sdl3.c.SDL_Window;

    pub fn modState() sdl3.c.SDL_Keymod {
        return sdl3.c.SDL_GetModState();
    }

    pub fn mouseState(x: *f32, y: *f32) sdl3.c.SDL_MouseButtonFlags {
        return sdl3.c.SDL_GetMouseState(x, y);
    }

    /// Whether the key at this SDL_Scancode is held now.
    pub fn keyDown(scancode: usize) bool {
        var count: c_int = 0;
        const keys = sdl3.c.SDL_GetKeyboardState(&count) orelse return false;
        if (scancode >= @as(usize, @intCast(count))) return false;
        return keys[scancode];
    }

    pub fn hasMouseFocus(window: Window) bool {
        return sdl3.c.SDL_GetMouseFocus() == window;
    }

    pub fn capture() view_math.Capture {
        return captureFlags();
    }
};

/// The app's view: SDL and ImGui input. Every method that talks to the
/// engine takes the bridge as `real: anytype` - `*RealBridge` in the app
/// (c_bridge.zig), a recording fake in the tests below.
pub const View = ViewWith(SdlInput);

pub fn ViewWith(comptime Input: type) type {
    return struct {
        const Self = @This();

        camera_x: f32 = 0,
        camera_y: f32 = 0,
        /// The bridge's own step count and scale (BkEditorViewState), kept in
        /// sync after every zoom (`syncFromBridge`): 0 steps, scale 1 is the
        /// game's unzoomed view.
        zoom_steps: i32 = 0,
        scale: f32 = 1,
        /// The Settings window's "Scroll and swipe speed" (D-25): a multiplier
        /// on a plain pan's pixels-per-unit gain, applied in `handleWheel`.
        /// Defaults to view_math's own constant until the app loads (or changes)
        /// `core.settings.Settings.scroll_speed`.
        wheel_sensitivity: f32 = view_math.wheel_sensitivity,
        /// Carries a Shift+wheel/swipe's fractional delta between events.
        zoom_wheel: view_math.ZoomWheel = .{},
        /// Carries a trackpad pinch's fractional delta between events; reset at
        /// the start and end of each gesture.
        pinch_zoom: view_math.PinchZoom = .{},
        /// Every map's camera and zoom for the current session (D-15), keyed by
        /// its document path; owned copies of the keys, freed in `deinit`.
        remembered: std.StringHashMapUnmanaged(SavedView) = .empty,
        /// The currently open map's path, as `showMap` was last called with -
        /// what `remembered` is saved under when another map replaces it.
        current_path: std.ArrayListUnmanaged(u8) = .empty,
        tool: Tool = .select,
        brush: tools.Brush,
        placer: tools.Placer,
        selector: tools.Selector = .{},
        /// 04-05: the Roads & Rivers tool (D-08).
        roads_rivers: core.tools_vso.RoadsRivers = .{},
        /// 04-06: the Bridge tool (D-10..D-12).
        bridge_tool: core.tools_groups.BridgeTool = .{},
        fence_tool: core.tools_groups.FenceTool = .{},
        /// 04-08: the Entrenchment tool (D-13).
        trench_tool: core.tools_groups.EntrenchmentTool = .{},
        /// 04-10: the Script Areas tool (D-21).
        areas_tool: core.tools_ai.ScriptAreas = .{},
        /// 04-11: the Start Target tool (D-17), entered from the Start Commands
        /// window for one click, and the tool it returns to.
        start_target: core.tools_ai.StartTarget = .{},
        start_target_return: Tool = .select,
        /// 04-11: the Reserve Positions tool (D-18), Unit -> Artillery positions mode.
        reserve_tool: core.tools_ai.ReservePositions = .{},
        /// 04-12: the AI General tool (D-19).
        ai_tool: core.tools_ai.AIGeneral = .{},
        /// Set each frame by a panel that uses the Delete key itself (the Start
        /// Commands window while it is focused): the view then does not hand
        /// Delete or Backspace to the tool, which would delete the selected
        /// object too. Cleared by `panels.draw` at the start of every frame.
        delete_claimed: bool = false,
        hover: ?tools.Pointer = null,

        map: view_math.MapSize = .{},
        panning: bool = false,
        pan_anchor: tools.Pointer = .{ .world_x = 0, .world_y = 0, .map_x = 0, .map_y = 0 },
        /// True between a left press the view saw and its release: an open
        /// tool gesture, which keeps the view routing motion and the release
        /// even if the cursor strays over an ImGui panel mid-drag.
        left_button_down: bool = false,
        /// The same for the right button - or for Ctrl+left, in a tool whose
        /// registry entry has `ctrl_click_is_right` (`right_via_ctrl`): the
        /// left button's own events then become right ones until its release.
        right_button_down: bool = false,
        right_via_ctrl: bool = false,
        /// The buttons a BK_EDITOR_AUTO schedule holds (SDL_BUTTON_*MASK
        /// bits): set by a scripted press and cleared by its scripted release
        /// (`holdScripted`). A scripted button is only an event pushed into
        /// SDL's queue - SDL_GetMouseState never sees it - so without this the
        /// stale-gesture guard in `update` would end a scripted drag at the
        /// end of the press's own frame, and the schedule's later `drag=` and
        /// `release=` would reach a tool with nothing in hand (04-05
        /// deviation 3, fixed in 04-06). Always 0 for a person at the mouse.
        scripted_buttons: u32 = 0,
        placer_name_storage: [64]u8 = undefined,
        /// The view's part of the status bar (see `statusLine`).
        status: view_math.StatusSlot = .{},
        allocator: std.mem.Allocator,

        pub fn init(allocator: std.mem.Allocator) Self {
            return .{
                .allocator = allocator,
                .brush = .{ .tile = 0 },
                .placer = .{ .name = "" },
            };
        }

        pub fn deinit(self: *Self, allocator: std.mem.Allocator) void {
            self.brush.deinit(allocator);
            self.roads_rivers.deinit(allocator);
            var keys = self.remembered.keyIterator();
            while (keys.next()) |key| allocator.free(key.*);
            self.remembered.deinit(allocator);
            self.current_path.deinit(allocator);
            self.* = undefined;
        }

        /// The view's own part of the status bar: "failed: " and the reason for
        /// anything worse than a refusal - a tool's or a panel's failed edit, or
        /// a lost frame main.zig reports. A refusal is not repeated here; the
        /// editor's own status already words it. Empty after the next edit that
        /// succeeds or is merely refused, so a failure never outlives the edits
        /// after it.
        pub fn statusLine(self: *const Self) []const u8 {
            return self.status.line();
        }

        /// True while a mouse gesture the view started (a left-button tool
        /// gesture, or a middle-button camera pan) is still open. main.zig's
        /// event loop uses this so a gesture that started on the map keeps
        /// reaching the view even if the cursor drifts over an ImGui panel
        /// before it ends.
        pub fn hasActiveMouseGesture(self: *const Self) bool {
            return self.left_button_down or self.right_button_down or self.panning;
        }

        /// The event with the modifier state stamped on its pointer.
        fn withCtrl(event: tools.Event, ctrl: bool) tools.Event {
            var stamped = event;
            switch (stamped) {
                .press, .drag, .release, .right_press, .right_drag, .right_release, .double_click => |*pointer| pointer.ctrl = ctrl,
                .key => {},
            }
            return stamped;
        }

        /// `error.Refused` is not an error here: the editor's status line
        /// already holds the reason. Anything else is shown too, prefixed
        /// "failed:". Exposed for main.zig to report failures view.zig itself
        /// did not cause (a lost GPU frame, say).
        pub fn setStatus(self: *Self, prefix: []const u8, message: []const u8) void {
            self.status.set(.general, prefix, message);
        }

        /// A message the operation that set it clears again once it succeeds
        /// (`clearStatusFrom`), with no edit needed in between.
        pub fn setStatusFrom(self: *Self, source: view_math.StatusSource, prefix: []const u8, message: []const u8) void {
            self.status.set(source, prefix, message);
        }

        pub fn clearStatus(self: *Self) void {
            self.status.clear();
        }

        /// Clears the message only if `source` set it.
        pub fn clearStatusFrom(self: *Self, source: view_math.StatusSource) void {
            self.status.clearFrom(source);
        }

        /// The status of an edit made anywhere - a tool, a menu, a panel -
        /// onto the view's part of the status bar (see `statusLine`).
        pub fn noteEditResult(self: *Self, editor: *Editor, result: EditError!void) void {
            result catch |err| return self.noteToolError(editor, err);
            self.clearStatus();
        }

        /// The tool palette's and the menu's way to switch tools: the same as
        /// the 1/2/3 keys, an open gesture on the old tool ended first.
        pub fn selectTool(self: *Self, editor: *Editor, tool: Tool) void {
            self.switchTool(editor, tool);
        }

        /// "Set target" of the Start Commands window: the Start Target tool for one
        /// click on command `index`, returning to the tool in hand afterwards.
        pub fn beginStartTarget(self: *Self, editor: *Editor, index: usize) void {
            if (self.tool != .start_target) self.start_target_return = self.tool;
            self.switchTool(editor, .start_target);
            self.start_target.index = index;
            self.start_target.done = false;
        }

        /// The object palette's choice: the placer's object from now on. The
        /// name is copied, so the caller's buffer may go.
        pub fn setPlacerObject(self: *Self, name: []const u8) void {
            const len = @min(name.len, self.placer_name_storage.len);
            @memcpy(self.placer_name_storage[0..len], name[0..len]);
            self.placer.name = self.placer_name_storage[0..len];
        }

        /// A scripted press (`down`) or release of the buttons in `mask`: held
        /// across frames for the stale-gesture guard until the scripted
        /// release, as a real held button would be (see `scripted_buttons`).
        pub fn holdScripted(self: *Self, mask: u32, down: bool) void {
            if (down) self.scripted_buttons |= mask else self.scripted_buttons &= ~mask;
        }

        /// Undo and redo for the Edit menu, reported as the keys report them.
        pub fn undo(self: *Self, editor: *Editor) void {
            self.runUndoable(editor, .undo);
        }

        pub fn redo(self: *Self, editor: *Editor) void {
            self.runUndoable(editor, .redo);
        }

        /// Called after every `editor.open` that succeeds: saves the outgoing
        /// map's camera and zoom under its own path (D-15), then either restores
        /// `path`'s remembered view or centres it unzoomed - a fresh map, or the
        /// first one this session, always opens on its middle at zoom 0, exactly
        /// what `BkEditorOpenMap` itself already put the camera and zoom at, so
        /// this is belt-and-suspenders for the "not remembered" branch and the
        /// actual behavior for a reopened one. Also picks `default_object` (the
        /// catalogue's first entry of game type unit, State's own - Task 3,
        /// carried from plan 5: this used to ask the bridge for the whole
        /// catalogue again just to find it) as the placer's default if none is
        /// chosen yet.
        pub fn showMap(self: *Self, real: anytype, path: []const u8, info: MapInfo, default_object: ?[]const u8) void {
            self.saveCurrentView();
            self.roads_rivers.reset();
            self.bridge_tool.reset();
            self.fence_tool.reset();
            self.trench_tool.reset();
            self.areas_tool.reset();
            self.start_target.reset();
            self.reserve_tool.reset();
            self.ai_tool.reset();
            // IN-C04: a scripted press left held (a failed pushMotion) must not
            // keep the stale-gesture guard off on the next map.
            self.scripted_buttons = 0;
            self.map = .{ .width_tiles = info.width_tiles, .height_tiles = info.height_tiles };
            if (self.remembered.get(path)) |saved| {
                self.camera_x = saved.camera_x;
                self.camera_y = saved.camera_y;
                _ = real.setCamera(self.camera_x, self.camera_y);
                _ = real.setZoom(saved.zoom_steps);
            } else {
                self.camera_x = @as(f32, @floatFromInt(info.width_tiles)) * world_cell_size / 2;
                self.camera_y = @as(f32, @floatFromInt(info.height_tiles)) * world_cell_size / 2;
                _ = real.setCamera(self.camera_x, self.camera_y);
                _ = real.setZoom(0);
            }
            self.syncFromBridge(real);
            self.current_path.clearRetainingCapacity();
            self.current_path.appendSlice(self.allocator, path) catch self.current_path.clearRetainingCapacity();
            if (self.placer.name.len == 0) {
                if (default_object) |name| self.setPlacerObject(name);
            }
        }

        /// The map was closed (File > Mod, D-26 revised 2026-09-29): the view is
        /// what it is with no map open at startup - no map size to clamp the
        /// camera to, no current path, no open gesture or hover. The closed
        /// map's camera and zoom are remembered first (D-15), so reopening it
        /// this session lands where it was. The placer forgets its object: it
        /// named an entry of the old object database, which the switch has just
        /// replaced - the next `showMap` picks the new catalogue's default.
        pub fn closeMap(self: *Self) void {
            self.saveCurrentView();
            self.current_path.clearRetainingCapacity();
            self.map = .{};
            self.scripted_buttons = 0; // IN-C04
            self.hover = null;
            self.panning = false;
            self.left_button_down = false;
            self.right_button_down = false;
            self.right_via_ctrl = false;
            self.zoom_wheel = .{};
            self.pinch_zoom = .{};
            self.selector = .{};
            self.brush.gesture = 0;
            self.brush.painted.clearRetainingCapacity();
            self.placer.name = "";
            self.roads_rivers.reset();
            self.bridge_tool.reset();
            self.fence_tool.reset();
            self.trench_tool.reset();
            self.areas_tool.reset();
            self.start_target.reset();
            self.reserve_tool.reset();
            self.ai_tool.reset();
        }

        /// Records `current_path`'s camera and zoom into `remembered`, if a map
        /// was open. An out-of-memory here just means that map's view is not
        /// remembered this time - never a reason to fail the map switch itself.
        fn saveCurrentView(self: *Self) void {
            const key = self.current_path.items;
            if (key.len == 0) return;
            const gop = self.remembered.getOrPut(self.allocator, key) catch return;
            if (!gop.found_existing) {
                gop.key_ptr.* = self.allocator.dupe(u8, key) catch {
                    _ = self.remembered.remove(key);
                    return;
                };
            }
            gop.value_ptr.* = .{ .camera_x = self.camera_x, .camera_y = self.camera_y, .zoom_steps = self.zoom_steps };
        }

        /// An event `main.zig` decided (through `inputKindOf`/`shouldDeliver`)
        /// belongs to the view. Any edit's outcome goes through
        /// `noteEditResult`: a refusal is left to the editor's own status, a
        /// failure is prefixed "failed:".
        pub fn handleEvent(self: *Self, editor: *Editor, real: anytype, event: *const sdl3.c.SDL_Event) void {
            switch (event.type) {
                sdl3.c.SDL_EVENT_MOUSE_BUTTON_DOWN, sdl3.c.SDL_EVENT_MOUSE_BUTTON_UP => {
                    const button = event.button;
                    if (button.button == sdl3.c.SDL_BUTTON_MIDDLE) {
                        self.handleMiddleButton(editor, button);
                        return;
                    }
                    const spec = tool_registry.entry(self.tool);
                    const kind = view_math.kindOf(.{ .button = button.button, .down = button.down, .clicks = button.clicks, .wants_double_click = spec.needs_double_click }) orelse return;
                    switch (kind) {
                        .press => {
                            // A press that cannot resolve starts no gesture: the
                            // tool has nothing to press on.
                            const pointer = editor.resolve(button.x, button.y) catch return;
                            self.hover = pointer;
                            // Ctrl+left is the right button in the tools that
                            // ask for it (macOS trackpads have one button).
                            if (spec.needs_right_button and spec.ctrl_click_is_right and Input.modState() & sdl3.c.SDL_KMOD_CTRL != 0) {
                                self.right_button_down = true;
                                self.right_via_ctrl = true;
                                self.dispatch(editor, .{ .right_press = pointer });
                                return;
                            }
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
                            if (self.right_via_ctrl) {
                                self.right_button_down = false;
                                self.right_via_ctrl = false;
                                self.dispatch(editor, .{ .right_release = pointer });
                                return;
                            }
                            self.left_button_down = false;
                            self.dispatch(editor, .{ .release = pointer });
                        },
                        .right_press => {
                            if (!spec.needs_right_button) return;
                            const pointer = editor.resolve(button.x, button.y) catch return;
                            self.hover = pointer;
                            self.right_button_down = true;
                            self.right_via_ctrl = false;
                            self.dispatch(editor, .{ .right_press = pointer });
                        },
                        .right_release => {
                            if (!self.right_button_down or self.right_via_ctrl) return;
                            const pointer = editor.resolve(button.x, button.y) catch (self.hover orelse tools.Pointer{ .world_x = 0, .world_y = 0, .map_x = 0, .map_y = 0 });
                            self.hover = pointer;
                            self.right_button_down = false;
                            self.dispatch(editor, .{ .right_release = pointer });
                        },
                        .double_click => {
                            // After the single click's own press and release.
                            if (!spec.needs_double_click) return;
                            const pointer = editor.resolve(button.x, button.y) catch return;
                            self.hover = pointer;
                            self.dispatch(editor, .{ .double_click = pointer });
                        },
                    }
                },
                sdl3.c.SDL_EVENT_MOUSE_MOTION => self.handleMotion(editor, real, event.motion),
                sdl3.c.SDL_EVENT_MOUSE_WHEEL => self.handleWheel(real, event.wheel),
                sdl3.c.SDL_EVENT_KEY_DOWN => self.handleKey(editor, real, event.key),
                sdl3.c.SDL_EVENT_PINCH_BEGIN, sdl3.c.SDL_EVENT_PINCH_END => self.pinch_zoom.reset(),
                sdl3.c.SDL_EVENT_PINCH_UPDATE => self.handlePinch(real, event.pinch),
                else => {},
            }
        }

        fn handleMiddleButton(self: *Self, editor: *Editor, button: sdl3.c.SDL_MouseButtonEvent) void {
            if (button.down) {
                self.pan_anchor = editor.resolve(button.x, button.y) catch return;
                self.panning = true;
            } else {
                self.panning = false;
            }
        }

        fn handleMotion(self: *Self, editor: *Editor, real: anytype, motion: sdl3.c.SDL_MouseMotionEvent) void {
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
                if (self.tool == .roads_rivers) self.roads_rivers.hoverNone();
                if (self.tool == .entrenchment) self.trench_tool.hoverNone();
                return;
            };
            self.hover = pointer;
            // Plain motion: Roads & Rivers follows the pointer (the unfinished
            // line's last leg, the control point Insert and Delete act on).
            // Not an edit, so it goes around dispatch and its status handling.
            if (self.tool == .roads_rivers and !self.left_button_down and !self.right_button_down) self.roads_rivers.hover(editor, pointer);
            // The Entrenchment tool's preview follows the pointer, and so does
            // the entrenchment under it (04-08).
            if (self.tool == .entrenchment and !self.left_button_down and !self.right_button_down) self.trench_tool.hover(editor, pointer);
            if (self.right_button_down) {
                // A right gesture (its own button, or Ctrl+left) drags as a
                // right one; the left button's motion is then the same gesture.
                const held = if (self.right_via_ctrl) view_math.sdl_button_lmask else view_math.sdl_button_rmask;
                if (motion.state & held != 0) self.dispatch(editor, .{ .right_drag = pointer });
                return;
            }
            if (motion.state & sdl3.c.SDL_BUTTON_LMASK != 0) self.dispatch(editor, .{ .drag = pointer });
        }

        /// A mouse wheel or a two-finger trackpad swipe pans the camera; with
        /// Shift held it zooms instead (D-10), like the game. SDL sends a swipe
        /// as many small fractional wheel events on both axes.
        /// view_math.wheelPan maps each straight to a screen pan, so the camera
        /// follows the fingers without rounding or stepping back; view_math's
        /// ZoomWheel folds the same fractional deltas into whole zoom steps.
        fn handleWheel(self: *Self, real: anytype, wheel: sdl3.c.SDL_MouseWheelEvent) void {
            if (Input.modState() & sdl3.c.SDL_KMOD_SHIFT != 0) {
                const delta = view_math.zoomDelta(wheel.x, wheel.y);
                const steps = self.zoom_wheel.feed(delta);
                if (steps == 0) return;
                if (real.zoomAt(steps, wheel.mouse_x, wheel.mouse_y) == .ok) self.syncFromBridge(real);
                return;
            }
            const pan = view_math.wheelPan(.{ .x = wheel.x, .y = wheel.y, .flipped = wheel.direction == sdl3.c.SDL_MOUSEWHEEL_FLIPPED }, self.wheel_sensitivity);
            // panScreen treats its pixels as world units 1:1 - true only at
            // scale 1. Dividing by the bridge's own scale first is what makes
            // the map follow the fingers 1:1 on screen at any zoom, the same
            // gain a mouse-driven drag already gets for free through
            // editor.resolve's screen-to-world conversion.
            const scale = if (self.scale > 0) self.scale else 1;
            const before_x = self.camera_x;
            const before_y = self.camera_y;
            var camera: view_math.Camera = .{ .x = self.camera_x, .y = self.camera_y };
            camera.panScreen(pan.right_px / scale, pan.up_px / scale, self.map);
            self.camera_x = camera.x;
            self.camera_y = camera.y;
            if (self.camera_x != before_x or self.camera_y != before_y) _ = real.setCamera(self.camera_x, self.camera_y);
        }

        /// A trackpad pinch update (D-10): folds the gesture's scale-since-last
        /// update into whole zoom steps and, on a whole step, zooms at wherever
        /// the pointer is now (SDL's pinch event carries no position of its
        /// own).
        fn handlePinch(self: *Self, real: anytype, pinch: sdl3.c.SDL_PinchFingerEvent) void {
            const steps = self.pinch_zoom.feed(pinch.scale);
            if (steps == 0) return;
            var mouse_x: f32 = 0;
            var mouse_y: f32 = 0;
            _ = Input.mouseState(&mouse_x, &mouse_y);
            if (real.zoomAt(steps, mouse_x, mouse_y) == .ok) self.syncFromBridge(real);
        }

        /// After a zoom (Shift+wheel, pinch, Home, or a remembered view
        /// restored): the camera, the zoom step and the scale all follow the
        /// bridge's own view state, which is where the zoom-at recipe left them
        /// (D-14's anchor shift moves the camera too, not just the zoom).
        pub fn syncFromBridge(self: *Self, real: anytype) void {
            const view = real.viewState() orelse return;
            self.camera_x = view.anchor_x;
            self.camera_y = view.anchor_y;
            self.zoom_steps = view.zoom_steps;
            self.scale = view.scale;
        }

        /// Q/E and the 1/2/3 tool keys ignore SDL's key-repeat (rotating by 16
        /// steps or hopping through tools because a key was held would surprise
        /// more than it would help). Delete/Backspace and undo/redo keep repeat:
        /// holding Cmd/Ctrl+Z to walk back several edits, or Delete to keep
        /// pressing it while nothing is selected, are both ordinary editor
        /// habits, and a delete or undo that repeats onto nothing just answers
        /// `error.Refused` harmlessly. Home (D-13) resets zoom and rotation to
        /// the game's default view - no modifier is checked, matching the menu's
        /// "Reset view" item, which carries the same shortcut label.
        fn handleKey(self: *Self, editor: *Editor, real: anytype, key: sdl3.c.SDL_KeyboardEvent) void {
            const command_or_control = key.mod & (sdl3.c.SDL_KMOD_CTRL | sdl3.c.SDL_KMOD_GUI) != 0;
            switch (key.key) {
                sdl3.c.SDLK_DELETE, sdl3.c.SDLK_BACKSPACE => if (!self.delete_claimed) self.dispatch(editor, .{ .key = .delete }),
                sdl3.c.SDLK_Q => if (!key.repeat) self.dispatch(editor, .{ .key = .rotate_left }),
                sdl3.c.SDLK_E => if (!key.repeat) self.dispatch(editor, .{ .key = .rotate_right }),
                sdl3.c.SDLK_RETURN, sdl3.c.SDLK_KP_ENTER => if (!key.repeat) self.dispatch(editor, .{ .key = .enter }),
                sdl3.c.SDLK_INSERT => if (!key.repeat) self.dispatch(editor, .{ .key = .insert }),
                sdl3.c.SDLK_ESCAPE => if (!key.repeat) self.dispatch(editor, .{ .key = .escape }),
                sdl3.c.SDLK_SPACE => if (!key.repeat) self.dispatch(editor, .{ .key = .space }),
                sdl3.c.SDLK_Z => if (command_or_control) {
                    if (key.mod & sdl3.c.SDL_KMOD_SHIFT != 0) self.runUndoable(editor, .redo) else self.runUndoable(editor, .undo);
                },
                sdl3.c.SDLK_Y => if (command_or_control) self.runUndoable(editor, .redo),
                sdl3.c.SDLK_HOME => self.resetView(real),
                // The registry's bare-digit shortcuts: 1, 2, 3 and the keys
                // the M2 tools take (4-9).
                else => if (!key.repeat) {
                    if (tool_registry.byShortcut(key.key)) |id| self.switchTool(editor, id);
                },
            }
        }

        /// Home and the "View > Reset view" menu item: back to the game's
        /// default zoom (D-13), anchored at the screen's centre the way
        /// `BkEditorSetZoom` always is - it does not re-centre the pan onto the
        /// map's middle, only undoes the zoom.
        pub fn resetView(self: *Self, real: anytype) void {
            if (real.setZoom(0) == .ok) self.syncFromBridge(real);
        }

        /// Puts the world point (x, y) at the middle of the view: a camera
        /// anchor's or an area's "Go to". Clamped to the map like every other
        /// camera move.
        pub fn centreOn(self: *Self, real: anytype, x: f32, y: f32) void {
            var camera: view_math.Camera = .{ .x = x, .y = y };
            camera.clamp(self.map);
            self.camera_x = camera.x;
            self.camera_y = camera.y;
            _ = real.setCamera(self.camera_x, self.camera_y);
        }

        /// The largest square the brush tool paints (tools.Brush.radius, 0-4),
        /// in corners along one side of that square of cells.
        const max_brush_corners: usize = 2 * 4 + 2;
        /// The perimeter of that square has 4*(n-1) corners for n corners per
        /// side (a plain rectangle's corner count, walked as one loop); the
        /// buffer is sized for the largest brush the slider allows.
        const max_outline_points: usize = 4 * (max_brush_corners - 1);

        /// A bright, easy-to-see outline colour, converted once through ImGui's
        /// own packer rather than hand-assuming its byte order.
        fn outlineColor() imgui.c.ImU32 {
            return imgui.c.igColorConvertFloat4ToU32(.{ .x = 1, .y = 1, .z = 0, .w = 1 });
        }

        /// A sound marker's own colour - distinct from the brush outline's
        /// yellow, so the two are never confused when both happen to be visible.
        fn soundMarkerColor() imgui.c.ImU32 {
            return imgui.c.igColorConvertFloat4ToU32(.{ .x = 0.25, .y = 0.65, .z = 1.0, .w = 1 });
        }

        /// Each sound is marked on the map: a small diamond at its position
        /// (BkEditorWorldToScreen) with its name beside it, the selected one
        /// highlighted in the brush outline's own colour. Skipped, per sound,
        /// when the conversion fails - off the current view, or no camera yet.
        fn drawSoundMarkers(real: anytype, sounds: []const core.bridge.SoundRecord, selected_sound: ?usize) void {
            if (sounds.len == 0) return;
            const draw_list = imgui.c.igGetBackgroundDrawList();
            for (sounds, 0..) |sound, index| {
                const screen = real.worldToScreen(sound.x, sound.y) orelse continue;
                const selected = selected_sound != null and selected_sound.? == index;
                const half: f32 = if (selected) 7.0 else 5.0;
                const color = if (selected) outlineColor() else soundMarkerColor();
                const top: imgui.c.ImVec2 = .{ .x = screen[0], .y = screen[1] - half };
                const right: imgui.c.ImVec2 = .{ .x = screen[0] + half, .y = screen[1] };
                const bottom: imgui.c.ImVec2 = .{ .x = screen[0], .y = screen[1] + half };
                const left: imgui.c.ImVec2 = .{ .x = screen[0] - half, .y = screen[1] };
                imgui.c.ImDrawList_AddQuadFilled(draw_list, top, right, bottom, left, color);
                imgui.c.ImDrawList_AddQuad(draw_list, top, right, bottom, left, outlineColor());
                const name = std.mem.sliceTo(&sound.name, 0);
                imgui.c.ImDrawList_AddTextEx(draw_list, .{ .x = screen[0] + half + 3, .y = screen[1] - half }, outlineColor(), name.ptr, name.ptr + name.len);
            }
        }

        /// Where the brush will paint, drawn on the terrain under the pointer at
        /// every zoom (carried from plan 5): with the brush tool active, a
        /// hovered tile, and the pointer not over a panel, the outline of the
        /// brush's square of cells as a closed polyline through every corner on
        /// its boundary - not just its four outer corners - so the line follows
        /// the ground the way BkEditorWorldToScreen sees it, on sloped terrain.
        /// Nothing is drawn if any corner fails to convert (off the map, or no
        /// camera). The sound markers above draw regardless of the active tool.
        pub fn drawOverlay(self: *Self, real: anytype, sounds: []const core.bridge.SoundRecord, selected_sound: ?usize) void {
            drawSoundMarkers(real, sounds, selected_sound);
            if (self.tool != .brush) return;
            if (Input.capture().mouse) return;
            const hover = self.hover orelse return;
            const tile = hover.tile orelse return;
            // The stamp's top-left, not a radius: the brush is sized in
            // cells per axis now (M3, D-22), even sizes included.
            const origin = tools.Brush.topLeft(self.brush.size, tile);
            const n: i32 = self.brush.size + 1; // corners along one side
            // The engine tier confirms this against BkEditorWorldToTile: a
            // tile's CENTRE - not a corner - is a plain index * world_cell_size
            // in X (CTerrain::GetTileIndex rounds to the nearest tile rather
            // than flooring), but (height_tiles - row) * world_cell_size in Y,
            // which it measures from the terrain's far edge, not world_y 0. A
            // cell's corner sits half a cell off its centre either way - the
            // -0.5 baked into base_x/base_y below, kept in these "corner index"
            // units rather than converted to world units yet, so every corner
            // along the boundary is a whole step of 1.0 from the last.
            const base_x = @as(f32, @floatFromInt(origin[0])) - 0.5;
            const base_y = @as(f32, @floatFromInt(origin[1])) - 0.5;
            const height_tiles = @as(f32, @floatFromInt(self.map.height_tiles));

            var points: [max_outline_points]imgui.c.ImVec2 = undefined;
            var count: usize = 0;
            // Walks the rectangle's perimeter once, clockwise from the top-left
            // corner, never repeating the corner it started at (AddPolyline's
            // Closed flag draws that last edge itself).
            var x: i32 = 0;
            while (x < n) : (x += 1) {
                if (!addCorner(real, &points, &count, base_x + toF(x), base_y, height_tiles)) return;
            }
            var y: i32 = 1;
            while (y < n) : (y += 1) {
                if (!addCorner(real, &points, &count, base_x + toF(n - 1), base_y + toF(y), height_tiles)) return;
            }
            x = n - 2;
            while (x >= 0) : (x -= 1) {
                if (!addCorner(real, &points, &count, base_x + toF(x), base_y + toF(n - 1), height_tiles)) return;
            }
            y = n - 2;
            while (y >= 1) : (y -= 1) {
                if (!addCorner(real, &points, &count, base_x, base_y + toF(y), height_tiles)) return;
            }

            const draw_list = imgui.c.igGetBackgroundDrawList();
            imgui.c.ImDrawList_AddPolyline(draw_list, &points, @intCast(count), outlineColor(), 2.0, imgui.c.ImDrawFlags_Closed);
        }

        fn toF(value: i32) f32 {
            return @floatFromInt(value);
        }

        /// One outline corner: a corner index (`cx`, `cy` - a whole or
        /// half-integer offset from the map's own tile grid) to its world
        /// point, then to the screen point `points[count]` holds. False (and
        /// `count` unmoved) on any conversion failure.
        fn addCorner(real: anytype, points: []imgui.c.ImVec2, count: *usize, cx: f32, cy: f32, height_tiles: f32) bool {
            const wx = cx * world_cell_size;
            const wy = (height_tiles - cy) * world_cell_size;
            const screen = real.worldToScreen(wx, wy) orelse return false;
            points[count.*] = .{ .x = screen[0], .y = screen[1] };
            count.* += 1;
            return true;
        }

        /// Switches the active tool, ending an open left- or right-button gesture on the
        /// old one first: without this, painting or dragging with the mouse
        /// still held while pressing 1/2/3 would leave the old tool's gesture
        /// open (`Brush.gesture`/`Selector.gesture` non-zero), and the next
        /// press on the new tool would merge into it as one undo step.
        fn switchTool(self: *Self, editor: *Editor, tool: Tool) void {
            if (self.left_button_down) {
                const pointer = self.hover orelse tools.Pointer{ .world_x = 0, .world_y = 0, .map_x = 0, .map_y = 0 };
                self.dispatch(editor, .{ .release = pointer });
                self.left_button_down = false;
            }
            if (self.right_button_down) {
                const pointer = self.hover orelse tools.Pointer{ .world_x = 0, .world_y = 0, .map_x = 0, .map_y = 0 };
                self.dispatch(editor, .{ .right_release = pointer });
                self.right_button_down = false;
                self.right_via_ctrl = false;
            }
            self.tool = tool;
        }

        const UndoableDirection = enum { undo, redo };

        fn runUndoable(self: *Self, editor: *Editor, direction: UndoableDirection) void {
            // WR-B02: every tool that selects by list index notes what it has
            // selected before the replay and finds it again after, so an undo
            // or redo that shifts a list never leaves a Delete, Q or Enter
            // aimed at the item that took its place.
            self.roads_rivers.captureSelection(editor);
            self.bridge_tool.captureSelection(editor);
            self.trench_tool.captureSelection(editor);
            self.areas_tool.captureSelection(editor);
            self.reserve_tool.captureSelection(editor);
            self.ai_tool.captureSelection(editor);
            const result = switch (direction) {
                .undo => editor.undo(),
                .redo => editor.redo(),
            };
            self.roads_rivers.resolveSelection(editor);
            self.bridge_tool.resolveSelection(editor);
            self.trench_tool.resolveSelection(editor);
            self.areas_tool.resolveSelection(editor);
            self.reserve_tool.resolveSelection(editor);
            self.ai_tool.resolveSelection(editor);
            if (result) |_| self.clearStatus() else |err| self.noteToolError(editor, err);
        }

        fn dispatch(self: *Self, editor: *Editor, raw_event: tools.Event) void {
            // The modifier travels with the pointer (04-07): the Fence tool's
            // Ctrl flips a single fence. Read where the event is handled, so a
            // scripted event and a real one see the same thing.
            const event = withCtrl(raw_event, Input.modState() & sdl3.c.SDL_KMOD_CTRL != 0);
            const result = switch (self.tool) {
                .select => self.selector.handle(editor, event),
                .brush => self.brush.handle(editor, event),
                .place => self.placer.handle(editor, event),
                .roads_rivers => self.roads_rivers.handle(editor, event),
                .bridge => self.bridge_tool.handle(editor, event),
                .fence => self.fence_tool.handle(editor, event),
                .entrenchment => self.trench_tool.handle(editor, event),
                .script_areas => self.areas_tool.handle(editor, event),
                .start_target => self.start_target.handle(editor, event),
                .reserve_positions => self.reserve_tool.handle(editor, event),
                .ai_general => self.ai_tool.handle(editor, event),
            };
            self.noteEditResult(editor, result);
            // The Start Target tool takes one click: back to the tool it came from.
            if (self.tool == .start_target and self.start_target.done) {
                self.start_target.reset();
                self.tool = self.start_target_return;
            }
        }

        /// `error.Refused` is not an error here: the editor's status line
        /// already holds the reason, so the view's part is cleared. Anything
        /// else is shown, prefixed "failed:".
        fn noteToolError(self: *Self, editor: *Editor, err: EditError) void {
            switch (err) {
                error.Refused => self.clearStatus(),
                error.OutOfMemory => self.setStatus("failed: ", "out of memory"),
                error.Failed => self.setStatus("failed: ", editor.status()),
            }
        }

        fn scrollCamera(self: *Self, dir: view_math.Scroll, dt_seconds: f32) void {
            var camera: view_math.Camera = .{ .x = self.camera_x, .y = self.camera_y };
            camera.scroll(dir, dt_seconds, self.map);
            self.camera_x = camera.x;
            self.camera_y = camera.y;
        }

        fn panCamera(self: *Self, delta_x: f32, delta_y: f32) void {
            var camera: view_math.Camera = .{ .x = self.camera_x + delta_x, .y = self.camera_y + delta_y };
            camera.clamp(self.map);
            self.camera_x = camera.x;
            self.camera_y = camera.y;
        }

        /// Per frame: the stale-gesture guard, then keyboard and edge scrolling,
        /// then the camera. Keys only scroll when ImGui does not want the
        /// keyboard; edge-scrolling reads the mouse directly (SDL_GetMouseState),
        /// so it still works while the view itself never saw a motion event this
        /// frame - but only while our own window has mouse focus and ImGui does
        /// not want the mouse, or the cursor sitting over another window, or a
        /// panel's edge, would scroll the map underneath it.
        pub fn update(self: *Self, editor: *Editor, real: anytype, window: Input.Window, dt_seconds: f32) void {
            // One SDL_GetMouseState call for both the stale-gesture guard below
            // (needs the button mask, read every frame regardless of focus or
            // ImGui capture) and edge-scrolling further down (needs the position,
            // gated on focus/capture) - the position is simply unused when that
            // gate does not hold.
            var mouse_x: f32 = -1;
            var mouse_y: f32 = -1;
            // A scripted hold counts as held (BK_EDITOR_AUTO's drags span
            // frames); it is 0 unless a schedule pressed a button.
            const buttons = Input.mouseState(&mouse_x, &mouse_y) | self.scripted_buttons;
            const capture = Input.capture();
            // Task 2 (carried from plan 5): a pan or a left-button tool gesture
            // whose release ImGui or another window took, rather than the view
            // itself, would otherwise never end - the view's own press/release
            // handlers are the only place panning/left_button_down used to clear.
            // Ctrl+left as the right button holds the LEFT mask.
            const right_mask_held = buttons & (if (self.right_via_ctrl) view_math.sdl_button_lmask else view_math.sdl_button_rmask) != 0;
            const stale = view_math.staleGesture(if (right_mask_held) buttons | view_math.sdl_button_rmask else buttons & ~view_math.sdl_button_rmask, self.panning, self.left_button_down, self.right_button_down);
            if (stale.end_pan) self.panning = false;
            if (stale.end_left) {
                const pointer = self.hover orelse tools.Pointer{ .world_x = 0, .world_y = 0, .map_x = 0, .map_y = 0 };
                self.dispatch(editor, .{ .release = pointer });
                self.left_button_down = false;
            }
            if (stale.end_right) {
                const pointer = self.hover orelse tools.Pointer{ .world_x = 0, .world_y = 0, .map_x = 0, .map_y = 0 };
                self.dispatch(editor, .{ .right_release = pointer });
                self.right_button_down = false;
                self.right_via_ctrl = false;
            }
            // Stale hover over a panel: the hovered tile and the brush outline
            // (drawOverlay checks Input.capture().mouse itself) must disappear
            // while ImGui has the mouse and no gesture the view started is still
            // open - a gesture in progress keeps its last hover so its own
            // release still resolves correctly.
            if (capture.mouse and !self.hasActiveMouseGesture()) self.hover = null;

            var dir: view_math.Scroll = .{};
            if (!capture.keyboard) {
                if (Input.keyDown(scancodeIndex(sdl3.c.SDL_SCANCODE_LEFT)) or Input.keyDown(scancodeIndex(sdl3.c.SDL_SCANCODE_A))) dir.left = true;
                if (Input.keyDown(scancodeIndex(sdl3.c.SDL_SCANCODE_RIGHT)) or Input.keyDown(scancodeIndex(sdl3.c.SDL_SCANCODE_D))) dir.right = true;
                if (Input.keyDown(scancodeIndex(sdl3.c.SDL_SCANCODE_UP)) or Input.keyDown(scancodeIndex(sdl3.c.SDL_SCANCODE_W))) dir.up = true;
                if (Input.keyDown(scancodeIndex(sdl3.c.SDL_SCANCODE_DOWN)) or Input.keyDown(scancodeIndex(sdl3.c.SDL_SCANCODE_S))) dir.down = true;
            }
            if (!capture.mouse and Input.hasMouseFocus(window)) {
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
}

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
        sdl3.c.SDL_EVENT_PINCH_BEGIN, sdl3.c.SDL_EVENT_PINCH_UPDATE, sdl3.c.SDL_EVENT_PINCH_END => .pinch,
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

// -- Tests: SDL events through the view's own wiring (WINDOWS.md 2) ---------
//
// `zig build test-map-editor-view` compiles this file with the app's own SDL
// headers (types and constants only: no SDL function is referenced here, so
// none is linked), the core for the app's target, and a stand-in for
// editor_imgui that nothing below reaches. The tools edit the core's fake
// bridge through a real Editor; the camera goes to FakeCamera, which does
// what BkEditorSetCamera/SetZoom/ZoomAt/ViewState do to the engine's view.

const testing = std.testing;
const fake_bridge = core.fake_bridge;

/// The input state `ViewWith` reads besides the event: set by each test.
const FakeInput = struct {
    pub const Window = void;

    var mod: sdl3.c.SDL_Keymod = 0;
    var mouse_x: f32 = -1;
    var mouse_y: f32 = -1;
    var buttons: u32 = 0;
    var keys: [512]bool = [_]bool{false} ** 512;
    var focus: bool = true;
    var capture_flags: view_math.Capture = .{};

    fn reset() void {
        mod = 0;
        mouse_x = -1;
        mouse_y = -1;
        buttons = 0;
        keys = [_]bool{false} ** 512;
        focus = true;
        capture_flags = .{};
    }

    pub fn modState() sdl3.c.SDL_Keymod {
        return mod;
    }

    pub fn mouseState(x: *f32, y: *f32) u32 {
        x.* = mouse_x;
        y.* = mouse_y;
        return buttons;
    }

    pub fn keyDown(scancode: usize) bool {
        return scancode < keys.len and keys[scancode];
    }

    pub fn hasMouseFocus(window: Window) bool {
        _ = window;
        return focus;
    }

    pub fn capture() view_math.Capture {
        return capture_flags;
    }
};

const TestView = ViewWith(FakeInput);

/// What BkEditorViewState reports.
const FakeViewState = struct { anchor_x: f32, anchor_y: f32, zoom_steps: i32, scale: f32 };

/// The engine's camera as the bridge moves it: SetCamera puts the anchor,
/// SetZoom and ZoomAt set or add whole steps (0..max_zoom), each step
/// scaling the view by 1.2 (GFX.World.ZoomFactor's default).
const FakeCamera = struct {
    anchor_x: f32 = 0,
    anchor_y: f32 = 0,
    zoom_steps: i32 = 0,
    max_zoom: i32 = 6,
    set_camera_calls: u32 = 0,
    zoom_at_calls: u32 = 0,
    last_zoom_at: [2]f32 = .{ 0, 0 },
    screen: [2]i32 = .{ 800, 600 },

    pub fn setCamera(self: *FakeCamera, x: f32, y: f32) core.bridge.Status {
        self.anchor_x = x;
        self.anchor_y = y;
        self.set_camera_calls += 1;
        return .ok;
    }

    pub fn setZoom(self: *FakeCamera, steps: i32) core.bridge.Status {
        self.zoom_steps = std.math.clamp(steps, 0, self.max_zoom);
        return .ok;
    }

    pub fn zoomAt(self: *FakeCamera, steps: i32, x: f32, y: f32) core.bridge.Status {
        self.zoom_steps = std.math.clamp(self.zoom_steps + steps, 0, self.max_zoom);
        self.zoom_at_calls += 1;
        self.last_zoom_at = .{ x, y };
        return .ok;
    }

    pub fn viewState(self: *FakeCamera) ?FakeViewState {
        return .{ .anchor_x = self.anchor_x, .anchor_y = self.anchor_y, .zoom_steps = self.zoom_steps, .scale = std.math.pow(f32, 1.2, @floatFromInt(self.zoom_steps)) };
    }

    pub fn screenSize(self: *FakeCamera) ?[2]i32 {
        return self.screen;
    }
};

/// The core's fixture map (8x8 tiles of 32 world units, the screen is the
/// world, a T34 at 40,40) open in a real Editor, a view showing it, and the
/// camera. Heap-allocated so the Editor's pointer into the fake stays put.
const Rig = struct {
    fake: fake_bridge.FakeBridge,
    editor: Editor,
    view: TestView,
    camera: FakeCamera = .{},

    fn create() !*Rig {
        FakeInput.reset();
        const rig = try testing.allocator.create(Rig);
        errdefer testing.allocator.destroy(rig);
        rig.fake = try fake_bridge.fixture(testing.allocator);
        rig.editor = Editor.init(testing.allocator, rig.fake.bridge());
        rig.view = TestView.init(testing.allocator);
        rig.camera = .{};
        try rig.editor.open("fixture.bzm");
        rig.view.showMap(&rig.camera, "fixture.bzm", rig.editor.document.info, "T34");
        return rig;
    }

    fn destroy(rig: *Rig) void {
        rig.view.deinit(testing.allocator);
        rig.editor.deinit();
        rig.fake.deinit();
        testing.allocator.destroy(rig);
    }

    fn send(rig: *Rig, event: sdl3.c.SDL_Event) void {
        rig.view.handleEvent(&rig.editor, &rig.camera, &event);
    }
};

fn eventOf(event_type: anytype) sdl3.c.SDL_Event {
    var event = std.mem.zeroes(sdl3.c.SDL_Event);
    event.type = @intCast(event_type);
    return event;
}

fn mouseButton(button: u8, down: bool, x: f32, y: f32) sdl3.c.SDL_Event {
    var event = eventOf(if (down) sdl3.c.SDL_EVENT_MOUSE_BUTTON_DOWN else sdl3.c.SDL_EVENT_MOUSE_BUTTON_UP);
    event.button.button = button;
    event.button.down = down;
    event.button.x = x;
    event.button.y = y;
    return event;
}

fn mouseMotion(x: f32, y: f32, buttons: u32) sdl3.c.SDL_Event {
    var event = eventOf(sdl3.c.SDL_EVENT_MOUSE_MOTION);
    event.motion.x = x;
    event.motion.y = y;
    event.motion.state = buttons;
    return event;
}

fn wheelEvent(x: f32, y: f32, at_x: f32, at_y: f32) sdl3.c.SDL_Event {
    var event = eventOf(sdl3.c.SDL_EVENT_MOUSE_WHEEL);
    event.wheel.x = x;
    event.wheel.y = y;
    event.wheel.mouse_x = at_x;
    event.wheel.mouse_y = at_y;
    return event;
}

fn keyDown(key: u32, mod: sdl3.c.SDL_Keymod, repeat: bool) sdl3.c.SDL_Event {
    var event = eventOf(sdl3.c.SDL_EVENT_KEY_DOWN);
    event.key.key = key;
    event.key.mod = mod;
    event.key.repeat = repeat;
    event.key.down = true;
    return event;
}

fn pinchEvent(event_type: anytype, scale: f32) sdl3.c.SDL_Event {
    var event = eventOf(event_type);
    event.pinch.scale = scale;
    return event;
}

const button_left = view_math.sdl_button_left;
const button_middle = view_math.sdl_button_middle;
const button_right = view_math.sdl_button_right;

/// The fixture's middle, where showMap puts a map opened for the first
/// time: 8 tiles * 32 * sqrt(2) / 2 world units each way.
const fixture_centre: f32 = 181.01934;

test "view: a map opened for the first time shows its middle, unzoomed" {
    const rig = try Rig.create();
    defer rig.destroy();
    try testing.expectApproxEqAbs(fixture_centre, rig.view.camera_x, 0.001);
    try testing.expectApproxEqAbs(fixture_centre, rig.view.camera_y, 0.001);
    try testing.expectApproxEqAbs(fixture_centre, rig.camera.anchor_x, 0.001);
    try testing.expectEqual(@as(i32, 0), rig.camera.zoom_steps);
    try testing.expectEqualStrings("T34", rig.view.placer.name);
}

test "view: the 2 key, then a left press, drag and release paint two cells as one undo step" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(keyDown(sdl3.c.SDLK_2, 0, false));
    try testing.expectEqual(Tool.brush, rig.view.tool);
    rig.view.brush.tile = 5;
    // This test is the gesture and undo semantics, not the brush's size
    // (D-22 made the default 2x2, which would spread the stroke): pin 1x1.
    rig.view.brush.size = 1;

    rig.send(mouseButton(button_left, true, 40, 40));
    try testing.expect(rig.view.hasActiveMouseGesture());
    rig.send(mouseMotion(72, 40, view_math.sdl_button_lmask));
    rig.send(mouseButton(button_left, false, 72, 40));
    try testing.expect(!rig.view.hasActiveMouseGesture());
    try testing.expectEqual(@as(u8, 5), rig.fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 5), rig.fake.tile(2, 1));
    try testing.expectEqual(@as(u8, 0), rig.fake.tile(3, 1));

    // One stroke, one undo: Cmd+Z takes both cells back.
    rig.send(keyDown(sdl3.c.SDLK_Z, sdl3.c.SDL_KMOD_GUI, false));
    try testing.expectEqual(@as(u8, 0), rig.fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 0), rig.fake.tile(2, 1));
    // Ctrl+Y puts them back.
    rig.send(keyDown(sdl3.c.SDLK_Y, sdl3.c.SDL_KMOD_CTRL, false));
    try testing.expectEqual(@as(u8, 5), rig.fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 5), rig.fake.tile(2, 1));
}

test "view: motion without the left button only hovers; a press off the map starts nothing" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(keyDown(sdl3.c.SDLK_2, 0, false));
    rig.view.brush.tile = 7;
    rig.send(mouseMotion(40, 40, 0));
    const hover = rig.view.hover orelse return error.TestUnexpectedResult;
    try testing.expectEqual([2]i32{ 1, 1 }, hover.tile.?);
    try testing.expectEqual(@as(u8, 0), rig.fake.tile(1, 1));

    rig.send(mouseButton(button_left, true, 1000, 1000));
    try testing.expect(!rig.view.hasActiveMouseGesture());
    try testing.expect(!rig.editor.dirty());
}

test "view: the right button edits nothing" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(keyDown(sdl3.c.SDLK_3, 0, false));
    rig.send(mouseButton(button_right, true, 100, 100));
    rig.send(mouseButton(button_right, false, 100, 100));
    try testing.expect(!rig.editor.dirty());
    try testing.expect(!rig.view.hasActiveMouseGesture());
}

test "view: the 3 key and a click place the palette's object at the pointer" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(keyDown(sdl3.c.SDLK_3, 0, false));
    try testing.expectEqual(Tool.place, rig.view.tool);
    const before = rig.editor.document.objects.items.len;
    rig.send(mouseButton(button_left, true, 120, 90));
    rig.send(mouseButton(button_left, false, 120, 90));
    try testing.expectEqual(before + 1, rig.editor.document.objects.items.len);
    const placed = rig.editor.document.find(rig.editor.selection.?).?;
    try testing.expectEqualStrings("T34", placed.nameSlice());
    try testing.expectEqual(@as(f32, 120), placed.x);
    try testing.expectEqual(@as(f32, 90), placed.y);
}

test "view: select a unit, Q turns it a sixteenth left, a held E repeats nothing, Delete removes it, undo brings it back" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(keyDown(sdl3.c.SDLK_1, 0, false));
    rig.send(mouseButton(button_left, true, 40, 40));
    rig.send(mouseButton(button_left, false, 40, 40));
    try testing.expectEqual(@as(?i32, 1), rig.editor.selection);

    rig.send(keyDown(sdl3.c.SDLK_Q, 0, false));
    try testing.expectEqual(@as(i32, 65536 - 4096), rig.editor.document.find(1).?.dir);
    rig.send(keyDown(sdl3.c.SDLK_E, 0, true));
    try testing.expectEqual(@as(i32, 65536 - 4096), rig.editor.document.find(1).?.dir);
    rig.send(keyDown(sdl3.c.SDLK_E, 0, false));
    try testing.expectEqual(@as(i32, 0), rig.editor.document.find(1).?.dir);

    rig.send(keyDown(sdl3.c.SDLK_DELETE, 0, false));
    try testing.expect(rig.editor.document.find(1) == null);
    rig.send(keyDown(sdl3.c.SDLK_Z, sdl3.c.SDL_KMOD_CTRL, false));
    try testing.expect(rig.editor.document.find(1) != null);
    // Cmd+Shift+Z redoes the delete.
    rig.send(keyDown(sdl3.c.SDLK_Z, sdl3.c.SDL_KMOD_GUI | sdl3.c.SDL_KMOD_SHIFT, false));
    try testing.expect(rig.editor.document.find(1) == null);
}

test "view: a select drag moves the grabbed unit with the pointer" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(mouseButton(button_left, true, 44, 38));
    rig.send(mouseMotion(64, 58, view_math.sdl_button_lmask));
    rig.send(mouseButton(button_left, false, 64, 58));
    const tank = rig.editor.document.find(1).?;
    try testing.expectEqual(@as(f32, 60), tank.x);
    try testing.expectEqual(@as(f32, 60), tank.y);
}

test "view: a held 2 key does not switch tools on its repeats, and a switch mid-stroke ends the stroke" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(keyDown(sdl3.c.SDLK_2, 0, true));
    try testing.expectEqual(Tool.select, rig.view.tool);
    rig.send(keyDown(sdl3.c.SDLK_2, 0, false));
    rig.view.brush.tile = 3;
    rig.send(mouseButton(button_left, true, 40, 40));
    try testing.expect(rig.view.brush.gesture != 0);
    rig.send(keyDown(sdl3.c.SDLK_1, 0, false));
    try testing.expectEqual(Tool.select, rig.view.tool);
    try testing.expectEqual(@as(u32, 0), rig.view.brush.gesture);
    try testing.expect(!rig.view.hasActiveMouseGesture());
}

test "view: the middle button drags the map so the grabbed point stays under the pointer" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(mouseButton(button_middle, true, 100, 100));
    try testing.expect(rig.view.panning);
    rig.send(mouseMotion(80, 90, view_math.sdl_button_mmask));
    // The fake's screen is its world: 20 and 10 units moved, opposite to
    // the pointer.
    try testing.expectApproxEqAbs(fixture_centre + 20, rig.view.camera_x, 0.001);
    try testing.expectApproxEqAbs(fixture_centre + 10, rig.view.camera_y, 0.001);
    try testing.expectApproxEqAbs(fixture_centre + 20, rig.camera.anchor_x, 0.001);
    rig.send(mouseButton(button_middle, false, 80, 90));
    try testing.expect(!rig.view.panning);
    try testing.expect(!rig.editor.dirty());
}

test "view: a wheel notch up pans the map 20 px toward the screen's top, 28.28 world units along (-1, +1)" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(wheelEvent(0, 1, 400, 300));
    // Screen up is world (-x, +y) (the engine tier checks the same against
    // BkEditorScreenToWorld); 20 px up moves 2 * 20 / sqrt(2) along each.
    try testing.expectApproxEqAbs(@as(f32, 152.735), rig.view.camera_x, 0.001);
    try testing.expectApproxEqAbs(@as(f32, 209.304), rig.view.camera_y, 0.001);
    try testing.expectApproxEqAbs(@as(f32, 152.735), rig.camera.anchor_x, 0.001);
    try testing.expectApproxEqAbs(@as(f32, 209.304), rig.camera.anchor_y, 0.001);
    try testing.expectEqual(@as(i32, 0), rig.camera.zoom_steps);

    // A swipe right: 20 px right is 14.14 world units along (+1, +1).
    rig.send(wheelEvent(1, 0, 400, 300));
    try testing.expectApproxEqAbs(@as(f32, 166.877), rig.view.camera_x, 0.001);
    try testing.expectApproxEqAbs(@as(f32, 223.446), rig.view.camera_y, 0.001);
}

test "view: at zoom step 1 the same notch pans 20 screen px, fewer world units" {
    const rig = try Rig.create();
    defer rig.destroy();
    FakeInput.mod = sdl3.c.SDL_KMOD_LSHIFT;
    rig.send(wheelEvent(0, 1, 400, 300));
    FakeInput.mod = 0;
    try testing.expectApproxEqAbs(@as(f32, 1.2), rig.view.scale, 0.0001);
    rig.send(wheelEvent(0, 1, 400, 300));
    // 28.284 / 1.2 world units each way.
    try testing.expectApproxEqAbs(fixture_centre - 23.570, rig.view.camera_x, 0.001);
    try testing.expectApproxEqAbs(fixture_centre + 23.570, rig.view.camera_y, 0.001);
}

test "view: Shift + wheel zooms at the pointer instead of panning, Home resets the zoom" {
    const rig = try Rig.create();
    defer rig.destroy();
    FakeInput.mod = sdl3.c.SDL_KMOD_RSHIFT;
    const calls_before = rig.camera.set_camera_calls;
    rig.send(wheelEvent(0, 1, 300, 200));
    try testing.expectEqual(@as(u32, 1), rig.camera.zoom_at_calls);
    try testing.expectEqual([2]f32{ 300, 200 }, rig.camera.last_zoom_at);
    try testing.expectEqual(@as(i32, 1), rig.view.zoom_steps);
    try testing.expectEqual(calls_before, rig.camera.set_camera_calls);
    // A trackpad's fractions add up to a step before zooming again.
    rig.send(wheelEvent(0, 0.5, 300, 200));
    try testing.expectEqual(@as(u32, 1), rig.camera.zoom_at_calls);
    rig.send(wheelEvent(0, 0.5, 300, 200));
    try testing.expectEqual(@as(i32, 2), rig.view.zoom_steps);
    // Shift + a mouse wheel arrives on macOS as a horizontal scroll.
    rig.send(wheelEvent(-1, 0, 300, 200));
    try testing.expectEqual(@as(i32, 1), rig.view.zoom_steps);

    FakeInput.mod = 0;
    rig.send(keyDown(sdl3.c.SDLK_HOME, 0, false));
    try testing.expectEqual(@as(i32, 0), rig.view.zoom_steps);
    try testing.expectApproxEqAbs(@as(f32, 1), rig.view.scale, 0.0001);
}

test "view: a pinch zooms in whole steps at the pointer, and a new pinch starts from nothing" {
    const rig = try Rig.create();
    defer rig.destroy();
    FakeInput.mouse_x = 250;
    FakeInput.mouse_y = 150;
    rig.send(pinchEvent(sdl3.c.SDL_EVENT_PINCH_BEGIN, 1));
    rig.send(pinchEvent(sdl3.c.SDL_EVENT_PINCH_UPDATE, 1.2));
    try testing.expectEqual(@as(i32, 1), rig.view.zoom_steps);
    try testing.expectEqual([2]f32{ 250, 150 }, rig.camera.last_zoom_at);
    rig.send(pinchEvent(sdl3.c.SDL_EVENT_PINCH_UPDATE, 1.1));
    try testing.expectEqual(@as(i32, 1), rig.view.zoom_steps);
    rig.send(pinchEvent(sdl3.c.SDL_EVENT_PINCH_END, 1));
    // The half step carried from the last gesture is gone.
    rig.send(pinchEvent(sdl3.c.SDL_EVENT_PINCH_BEGIN, 1));
    rig.send(pinchEvent(sdl3.c.SDL_EVENT_PINCH_UPDATE, 1.1));
    try testing.expectEqual(@as(i32, 1), rig.view.zoom_steps);
}

test "view: the up arrow held for 0.1 s scrolls 141.4 world units along (-1, +1)" {
    const rig = try Rig.create();
    defer rig.destroy();
    FakeInput.keys[scancodeIndex(sdl3.c.SDL_SCANCODE_UP)] = true;
    FakeInput.focus = false;
    rig.view.update(&rig.editor, &rig.camera, {}, 0.1);
    try testing.expectApproxEqAbs(@as(f32, 39.598), rig.view.camera_x, 0.001);
    try testing.expectApproxEqAbs(@as(f32, 322.441), rig.view.camera_y, 0.001);
    try testing.expectApproxEqAbs(@as(f32, 39.598), rig.camera.anchor_x, 0.001);

    // While ImGui has the keyboard (typing in a panel), keys do not scroll.
    FakeInput.capture_flags = .{ .keyboard = true };
    const calls = rig.camera.set_camera_calls;
    rig.view.update(&rig.editor, &rig.camera, {}, 0.1);
    try testing.expectEqual(calls, rig.camera.set_camera_calls);
}

test "view: the pointer at the window's left edge scrolls left, only with focus and outside the panels" {
    const rig = try Rig.create();
    defer rig.destroy();
    FakeInput.mouse_x = 2;
    FakeInput.mouse_y = 300;
    rig.view.update(&rig.editor, &rig.camera, {}, 0.1);
    // Left is world (-1, -1): 70.71 each way.
    try testing.expectApproxEqAbs(@as(f32, 110.309), rig.view.camera_x, 0.001);
    try testing.expectApproxEqAbs(@as(f32, 110.309), rig.view.camera_y, 0.001);

    const calls = rig.camera.set_camera_calls;
    FakeInput.focus = false;
    rig.view.update(&rig.editor, &rig.camera, {}, 0.1);
    FakeInput.focus = true;
    FakeInput.capture_flags = .{ .mouse = true };
    rig.view.update(&rig.editor, &rig.camera, {}, 0.1);
    try testing.expectEqual(calls, rig.camera.set_camera_calls);
}

test "view: a pan or stroke whose release went elsewhere ends on the next frame" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(mouseButton(button_middle, true, 100, 100));
    FakeInput.buttons = view_math.sdl_button_mmask;
    rig.view.update(&rig.editor, &rig.camera, {}, 0);
    try testing.expect(rig.view.panning);
    FakeInput.buttons = 0;
    rig.view.update(&rig.editor, &rig.camera, {}, 0);
    try testing.expect(!rig.view.panning);

    rig.send(keyDown(sdl3.c.SDLK_2, 0, false));
    rig.view.brush.tile = 9;
    // Pin 1x1 like the stroke test above: D-22's 2x2 default would spread
    // this stroke into the cell the undo assertions expect to stay 0.
    rig.view.brush.size = 1;
    rig.send(mouseButton(button_left, true, 40, 40));
    try testing.expect(rig.view.brush.gesture != 0);
    rig.view.update(&rig.editor, &rig.camera, {}, 0);
    try testing.expect(!rig.view.hasActiveMouseGesture());
    try testing.expectEqual(@as(u32, 0), rig.view.brush.gesture);
    // A later press is a new stroke, a separate undo step.
    rig.send(mouseButton(button_left, true, 72, 40));
    rig.send(mouseButton(button_left, false, 72, 40));
    rig.send(keyDown(sdl3.c.SDLK_Z, sdl3.c.SDL_KMOD_GUI, false));
    try testing.expectEqual(@as(u8, 9), rig.fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 0), rig.fake.tile(2, 1));
}

test "view: a scripted press stays held across frames until its scripted release, so a drag spans frames as one stroke" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(keyDown(sdl3.c.SDLK_2, 0, false));
    rig.view.brush.tile = 7;
    // BK_EDITOR_AUTO's press: the real mouse holds nothing (FakeInput.buttons
    // stays 0), the schedule holds the left button.
    rig.view.holdScripted(view_math.sdl_button_lmask, true);
    rig.send(mouseButton(button_left, true, 40, 40));
    rig.view.update(&rig.editor, &rig.camera, {}, 0);
    try testing.expect(rig.view.hasActiveMouseGesture());
    // Later frames: the drags reach the same stroke.
    rig.send(mouseMotion(72, 40, view_math.sdl_button_lmask));
    rig.view.update(&rig.editor, &rig.camera, {}, 0);
    try testing.expect(rig.view.hasActiveMouseGesture());
    rig.send(mouseMotion(104, 40, view_math.sdl_button_lmask));
    rig.view.update(&rig.editor, &rig.camera, {}, 0);
    try testing.expect(rig.view.hasActiveMouseGesture());
    // The scripted release, in a later frame still.
    rig.view.holdScripted(view_math.sdl_button_lmask, false);
    rig.send(mouseButton(button_left, false, 104, 40));
    rig.view.update(&rig.editor, &rig.camera, {}, 0);
    try testing.expect(!rig.view.hasActiveMouseGesture());
    try testing.expectEqual(@as(u32, 0), rig.view.scripted_buttons);
    try testing.expectEqual(@as(u8, 7), rig.fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 7), rig.fake.tile(2, 1));
    try testing.expectEqual(@as(u8, 7), rig.fake.tile(3, 1));
    // One stroke, one undo step.
    try testing.expectEqual(@as(usize, 1), rig.editor.history.undo_stack.items.len);
    rig.send(keyDown(sdl3.c.SDLK_Z, sdl3.c.SDL_KMOD_GUI, false));
    try testing.expectEqual(@as(u8, 0), rig.fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 0), rig.fake.tile(3, 1));
    // Without the hold the guard still ends a stroke whose release went
    // elsewhere (the M1 rule is unchanged).
    rig.send(mouseButton(button_left, true, 40, 40));
    rig.view.update(&rig.editor, &rig.camera, {}, 0);
    try testing.expect(!rig.view.hasActiveMouseGesture());
}

test "view: a hover over a panel is dropped unless a gesture the view started is open" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(mouseMotion(40, 40, 0));
    try testing.expect(rig.view.hover != null);
    FakeInput.capture_flags = .{ .mouse = true };
    rig.view.update(&rig.editor, &rig.camera, {}, 0);
    try testing.expect(rig.view.hover == null);
}

test "view (D-15): each map's camera and zoom come back when it is reopened this session" {
    const rig = try Rig.create();
    defer rig.destroy();
    const info = rig.editor.document.info;
    // Map A: panned and zoomed in two steps.
    rig.view.showMap(&rig.camera, "maps/a.bzm", info, null);
    rig.send(wheelEvent(0, 1, 400, 300));
    FakeInput.mod = sdl3.c.SDL_KMOD_LSHIFT;
    rig.send(wheelEvent(0, 2, 400, 300));
    FakeInput.mod = 0;
    try testing.expectEqual(@as(i32, 2), rig.view.zoom_steps);
    const a_x = rig.view.camera_x;
    const a_y = rig.view.camera_y;
    try testing.expect(a_x != fixture_centre);

    // Map B, new this session: its middle, unzoomed.
    rig.view.showMap(&rig.camera, "maps/b.bzm", info, null);
    try testing.expectApproxEqAbs(fixture_centre, rig.view.camera_x, 0.001);
    try testing.expectEqual(@as(i32, 0), rig.view.zoom_steps);
    try testing.expectEqual(@as(i32, 0), rig.camera.zoom_steps);
    rig.send(wheelEvent(1, 0, 400, 300));
    const b_x = rig.view.camera_x;

    // A again: where it was left, at its zoom, in the view and the engine.
    rig.view.showMap(&rig.camera, "maps/a.bzm", info, null);
    try testing.expectEqual(a_x, rig.view.camera_x);
    try testing.expectEqual(a_y, rig.view.camera_y);
    try testing.expectEqual(@as(i32, 2), rig.view.zoom_steps);
    try testing.expectEqual(@as(i32, 2), rig.camera.zoom_steps);
    try testing.expectEqual(a_x, rig.camera.anchor_x);

    // Closing a map (File > Close, File > Mod) remembers it too.
    rig.view.closeMap();
    rig.view.showMap(&rig.camera, "maps/b.bzm", info, null);
    try testing.expectEqual(b_x, rig.view.camera_x);
    try testing.expectEqual(@as(i32, 0), rig.view.zoom_steps);
    rig.view.closeMap();
    rig.view.showMap(&rig.camera, "maps/a.bzm", info, null);
    try testing.expectEqual(a_x, rig.view.camera_x);
    try testing.expectEqual(@as(i32, 2), rig.view.zoom_steps);
}

test "view: a failed edit shows 'failed: ', the next good edit clears it; a tagged message waits for its own source" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.view.noteEditResult(&rig.editor, error.OutOfMemory);
    try testing.expectEqualStrings("failed: out of memory", rig.view.statusLine());
    rig.view.noteEditResult(&rig.editor, {});
    try testing.expectEqualStrings("", rig.view.statusLine());

    rig.view.setStatusFrom(.frame, "failed: ", "DeviceLost");
    rig.view.clearStatusFrom(.test_launch);
    try testing.expectEqualStrings("failed: DeviceLost", rig.view.statusLine());
    rig.view.clearStatusFrom(.frame);
    try testing.expectEqualStrings("", rig.view.statusLine());
}

test "view: centreOn puts the camera at the point, clamped to the map, and tells the engine" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.view.centreOn(&rig.camera, 100, 120);
    try testing.expectEqual(@as(f32, 100), rig.view.camera_x);
    try testing.expectEqual(@as(f32, 120), rig.view.camera_y);
    try testing.expectEqual(@as(f32, 100), rig.camera.anchor_x);
    try testing.expectEqual(@as(f32, 120), rig.camera.anchor_y);
    // Off the map: clamped like every other camera move.
    rig.view.centreOn(&rig.camera, -50, 100000);
    try testing.expectEqual(@as(f32, 0), rig.view.camera_x);
    try testing.expect(rig.view.camera_y < 100000);
    try testing.expectEqual(rig.view.camera_x, rig.camera.anchor_x);
    try testing.expectEqual(rig.view.camera_y, rig.camera.anchor_y);
}

fn doubleClickDown(x: f32, y: f32) sdl3.c.SDL_Event {
    var event = mouseButton(button_left, true, x, y);
    event.button.clicks = 2;
    return event;
}

test "view: the right button and the new keys reach no gesture in tools that do not ask for them" {
    const rig = try Rig.create();
    defer rig.destroy();
    for ([_]Tool{ .select, .brush, .place }) |tool| {
        rig.view.selectTool(&rig.editor, tool);
        const depth = rig.editor.history.undo_stack.items.len;
        rig.send(mouseButton(button_right, true, 40, 40));
        try testing.expect(!rig.view.hasActiveMouseGesture());
        rig.send(mouseMotion(60, 40, view_math.sdl_button_rmask));
        rig.send(mouseButton(button_right, false, 60, 40));
        for ([_]u32{ sdl3.c.SDLK_RETURN, sdl3.c.SDLK_KP_ENTER, sdl3.c.SDLK_INSERT, sdl3.c.SDLK_ESCAPE, sdl3.c.SDLK_SPACE, sdl3.c.SDLK_0 }) |key| {
            rig.send(keyDown(key, 0, false));
        }
        try testing.expectEqual(depth, rig.editor.history.undo_stack.items.len);
        try testing.expectEqual(tool, rig.view.tool);
        try testing.expect(!rig.view.hasActiveMouseGesture());
    }
}

test "view: key 4 is Roads & Rivers; clicks, a right click and a double click draw a road as one step" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(keyDown(sdl3.c.SDLK_4, 0, false));
    try testing.expectEqual(Tool.roads_rivers, rig.view.tool);
    rig.view.roads_rivers.setDesc("road_track");
    rig.send(mouseButton(button_left, true, 40, 40));
    rig.send(mouseButton(button_left, false, 40, 40));
    rig.send(mouseButton(button_left, true, 120, 40));
    rig.send(mouseButton(button_left, false, 120, 40));
    rig.send(mouseButton(button_left, true, 150, 150));
    rig.send(mouseButton(button_left, false, 150, 150));
    // The right button takes the last point back.
    rig.send(mouseButton(button_right, true, 150, 150));
    rig.send(mouseButton(button_right, false, 150, 150));
    try testing.expectEqual(@as(usize, 2), rig.view.roads_rivers.pending_len);
    rig.send(mouseButton(button_left, true, 200, 120));
    rig.send(mouseButton(button_left, false, 200, 120));
    rig.send(doubleClickDown(200, 120));
    var up = mouseButton(button_left, false, 200, 120);
    up.button.clicks = 2;
    rig.send(up);
    try testing.expectEqual(@as(usize, 1), rig.fake.vsoLen(.road));
    try testing.expectEqual(@as(usize, 3), rig.fake.vso(.road, 0).count);
    try testing.expectEqual(@as(f32, 200), rig.fake.vso(.road, 0).controls[2].x);
    try testing.expectEqual(@as(usize, 1), rig.editor.history.undo_stack.items.len);
    try testing.expect(!rig.view.hasActiveMouseGesture());
}

test "view: Ctrl+left is the right button in Roads & Rivers" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.view.selectTool(&rig.editor, .roads_rivers);
    rig.view.roads_rivers.setDesc("road_track");
    rig.send(mouseButton(button_left, true, 40, 40));
    rig.send(mouseButton(button_left, false, 40, 40));
    rig.send(mouseButton(button_left, true, 120, 40));
    rig.send(mouseButton(button_left, false, 120, 40));
    FakeInput.mod = sdl3.c.SDL_KMOD_CTRL;
    rig.send(mouseButton(button_left, true, 120, 40));
    try testing.expect(rig.view.right_button_down);
    rig.send(mouseButton(button_left, false, 120, 40));
    FakeInput.mod = 0;
    try testing.expectEqual(@as(usize, 1), rig.view.roads_rivers.pending_len);
    try testing.expect(!rig.view.hasActiveMouseGesture());
    rig.send(keyDown(sdl3.c.SDLK_ESCAPE, 0, false));
    try testing.expectEqual(@as(usize, 0), rig.view.roads_rivers.pending_len);
    try testing.expectEqual(@as(usize, 0), rig.editor.history.undo_stack.items.len);
}

test "view: Ctrl+left is only the right button in a tool that asks for it, so a select press is still a press" {
    const rig = try Rig.create();
    defer rig.destroy();
    FakeInput.mod = sdl3.c.SDL_KMOD_CTRL;
    rig.send(mouseButton(button_left, true, 40, 40));
    // Select does not ask: the press is an ordinary left press.
    try testing.expect(rig.view.left_button_down);
    try testing.expect(!rig.view.right_button_down);
    rig.send(mouseButton(button_left, false, 40, 40));
    try testing.expect(!rig.view.hasActiveMouseGesture());
}

test "view: in a tool with no use for a double click the second click of a fast pair is a click of its own (WR-C03)" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(mouseButton(button_left, true, 40, 40));
    rig.send(mouseButton(button_left, false, 40, 40));
    // SDL's clicks 2 (within 500 ms and 32 px): a press and a release in Select,
    // never swallowed.
    rig.send(doubleClickDown(40, 40));
    try testing.expect(rig.view.hasActiveMouseGesture());
    var up = mouseButton(button_left, false, 40, 40);
    up.button.clicks = 2;
    rig.send(up);
    try testing.expect(!rig.view.hasActiveMouseGesture());
    // Both clicks selected the tank; nothing was deselected or moved.
    try testing.expectEqual(@as(?i32, 1), rig.editor.selection);
    try testing.expectEqual(@as(usize, 0), rig.editor.history.undo_stack.items.len);
}

test "view: the registry's shortcuts still switch the M1 tools" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(keyDown(sdl3.c.SDLK_3, 0, false));
    try testing.expectEqual(Tool.place, rig.view.tool);
    rig.send(keyDown(sdl3.c.SDLK_2, 0, false));
    try testing.expectEqual(Tool.brush, rig.view.tool);
    rig.send(keyDown(sdl3.c.SDLK_1, 0, false));
    try testing.expectEqual(Tool.select, rig.view.tool);
    // Keys the registry does not know change nothing (7 is the Entrenchment
    // tool's since 04-08, 8 the Script Areas tool's since 04-10, 9 the AI General
    // tool's since 04-12; 0 is nobody's).
    rig.send(keyDown(sdl3.c.SDLK_0, 0, false));
    try testing.expectEqual(Tool.select, rig.view.tool);
    rig.send(keyDown(sdl3.c.SDLK_7, 0, false));
    try testing.expectEqual(Tool.entrenchment, rig.view.tool);
    // 8 is the Script Areas tool's since 04-10.
    rig.send(keyDown(sdl3.c.SDLK_8, 0, false));
    try testing.expectEqual(Tool.script_areas, rig.view.tool);
    // 9 is the AI General tool's since 04-12.
    rig.send(keyDown(sdl3.c.SDLK_9, 0, false));
    try testing.expectEqual(Tool.ai_general, rig.view.tool);
}

test "view: key 9 is the AI General tool: a click on open ground makes a parcel, Enter switches it, Delete removes it, each one step" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(keyDown(sdl3.c.SDLK_9, 0, false));
    try testing.expectEqual(Tool.ai_general, rig.view.tool);
    const depth = rig.editor.history.undo_stack.items.len;
    rig.send(mouseButton(button_left, true, 100, 100));
    rig.send(mouseButton(button_left, false, 100, 100));
    try testing.expectEqual(@as(usize, 1), rig.fake.ai_sides.items.len);
    try testing.expectEqual(@as(usize, 1), rig.fake.ai_sides.items[0].parcels.len);
    try testing.expectEqual(@as(f32, 256), rig.fake.ai_sides.items[0].parcels[0].radius);
    try testing.expectEqual(depth + 1, rig.editor.history.undo_stack.items.len);
    rig.send(keyDown(sdl3.c.SDLK_RETURN, 0, false));
    try testing.expectEqual(core.records.ParcelKind.reinforce, rig.fake.ai_sides.items[0].parcels[0].kind);
    try testing.expectEqual(depth + 2, rig.editor.history.undo_stack.items.len);
    rig.send(keyDown(sdl3.c.SDLK_DELETE, 0, false));
    try testing.expectEqual(@as(usize, 0), rig.fake.ai_sides.items[0].parcels.len);
    try testing.expectEqual(depth + 3, rig.editor.history.undo_stack.items.len);
    rig.view.undo(&rig.editor);
    try testing.expectEqual(@as(usize, 1), rig.fake.ai_sides.items[0].parcels.len);
}

test "view: Set target puts the Start Target tool in hand for one click, sets the point on the release and returns to the tool it came from" {
    const rig = try Rig.create();
    defer rig.destroy();
    try rig.fake.addStartCommandFixtureFull(.{ .cmd_type = 0, .link_id = 1, .x = 5, .y = 5, .units = &.{1} });
    rig.view.selectTool(&rig.editor, .brush);
    rig.view.beginStartTarget(&rig.editor, 0);
    try testing.expectEqual(Tool.start_target, rig.view.tool);
    // The press changes nothing; the click is the release, as in the MFC editor.
    rig.send(mouseButton(button_left, true, 100, 100));
    try testing.expectEqual(Tool.start_target, rig.view.tool);
    rig.send(mouseButton(button_left, false, 100, 100));
    try testing.expectEqual(Tool.brush, rig.view.tool);
    try testing.expectEqual(@as(i32, 0), rig.fake.start_commands.items[0].target);
    try testing.expectEqual(@as(f32, 100), rig.fake.start_commands.items[0].x);
    try testing.expectEqual(@as(f32, 100), rig.fake.start_commands.items[0].y);
    try testing.expectEqual(@as(usize, 1), rig.editor.history.undo_stack.items.len);
    // A second click is the brush's again, not the target's.
    try testing.expect(!rig.view.left_button_down);
    // Asked again from the Start Target tool itself, it still returns to the brush.
    rig.view.beginStartTarget(&rig.editor, 0);
    rig.view.beginStartTarget(&rig.editor, 0);
    rig.send(mouseButton(button_left, true, 40, 40));
    rig.send(mouseButton(button_left, false, 40, 40));
    try testing.expectEqual(Tool.brush, rig.view.tool);
    // Clicking the tank at 40,40 made it the target.
    try testing.expectEqual(@as(i32, 1), rig.fake.start_commands.items[0].target);
}

test "view: Delete is not the tool's while a panel has claimed it" {
    const rig = try Rig.create();
    defer rig.destroy();
    rig.send(mouseButton(button_left, true, 40, 40));
    rig.send(mouseButton(button_left, false, 40, 40));
    try testing.expectEqual(@as(?i32, 1), rig.editor.selection);
    rig.view.delete_claimed = true;
    rig.send(keyDown(sdl3.c.SDLK_DELETE, 0, false));
    try testing.expect(rig.editor.document.find(1) != null);
    rig.view.delete_claimed = false;
    rig.send(keyDown(sdl3.c.SDLK_DELETE, 0, false));
    try testing.expect(rig.editor.document.find(1) == null);
}
