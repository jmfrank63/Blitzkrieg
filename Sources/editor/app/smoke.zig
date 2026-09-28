//! MapEditor --smoke: the interactive loop (main.zig `run`), hidden, driven
//! by synthetic SDL events instead of a person. One table, `script`, holds
//! every step: what is pushed onto SDL's queue before a frame, and what must
//! be true of the editor after it. Plan 6's BK_EDITOR_AUTO generalises the
//! table; nothing else here knows which steps there are.
//!
//! Screen positions are offsets from the screen's centre, where State.init
//! centres the camera on the map's middle: the same map point on every
//! screen size (1280x800 on the macOS runner, 1008x681 on the Windows one),
//! and clear of the panels on both (the left panel ends at 280, the right
//! one starts 320 from the right edge). Each step asserts the edit it made
//! actually landed - a tile painted, an object added, the selection moved -
//! so events swallowed on the way (a panel's capture, a hidden window) fail
//! the smoke rather than pass it silently.
//!
//! Select, turn, drag and delete act on the object the script placed,
//! clicked where it was put: a unit, which the engine turns (it turns none
//! of the map's static objects near the centre). One click on an object of
//! the map keeps a loaded object's pick covered.
const std = @import("std");
const sdl3 = @import("sdl3");
const core = @import("editor_core");
const c_bridge = @import("c_bridge.zig");
const view_mod = @import("view.zig");
const view_math = @import("view_math.zig");
const panels = @import("panels.zig");
const panels_logic = @import("panels_logic.zig");

const sdl = sdl3.c;
const c = c_bridge.c;
const Editor = core.editor.Editor;
const Pose = core.editor.Pose;
const View = view_mod.View;
const RealBridge = c_bridge.RealBridge;

/// Pixels from the screen's centre.
pub const Pos = struct { dx: f32, dy: f32 };

pub const Key = struct {
    key: sdl.SDL_Keycode,
    scancode: sdl.SDL_Scancode,
    mod: sdl.SDL_Keymod = 0,
};

pub const Input = union(enum) {
    /// Key down and up.
    key: Key,
    /// Left button down, after a motion to the point as a real pointer
    /// would make.
    press: Pos,
    /// A motion with the left button held.
    drag: Pos,
    /// A motion to the point with the button held, then the button up.
    release: Pos,
    /// What the Save As and Open dialogs hand the panels (the dialog itself
    /// needs a person): the smoke's output path.
    save_as,
    open_saved,
    /// A motion to the point, then `count` wheel events of these deltas
    /// there, as a trackpad swipe sends them: small fractions, one per
    /// NSEvent.
    wheel: Wheel,
    /// Pushes SDL_EVENT_WINDOW_CLOSE_REQUESTED for the window, as clicking
    /// its close button would (D-23).
    window_close,
    /// Sets state.actions.save_requested, as the Save menu item would - to
    /// prove a shipped map's Save redirects to Save As (D-18) without a
    /// widget.
    save_requested,
    /// Calls the unsaved-changes prompt's answer as the modal's Save/Don't
    /// save/Cancel button would (D-23) - the smoke has no widget to click.
    answer: panels_logic.UnsavedPrompt.Choice,
    /// Calls panels.addSoundAtViewCentre directly, as the Sounds panel's
    /// "Add at view centre" button would - no widget here to click either.
    add_sound_at_view_centre,
};

pub const Wheel = struct {
    /// Where the pointer is; `over_left_panel` puts it inside the left
    /// panel instead, at the centre's height.
    at: Pos = .{ .dx = 0, .dy = 0 },
    over_left_panel: bool = false,
    x: f32,
    y: f32,
    count: u8,
    flipped: bool = false,
    /// Held for a Shift+wheel zoom step (D-10). Pushed key events do not
    /// change SDL's own modifier state (SDL_GetModState, which view.zig's
    /// handleWheel reads), so the script sets it directly before pushing the
    /// wheel events and restores it to none right after.
    mods: sdl.SDL_Keymod = 0,
};

/// What must be true after the step's frame.
pub const Expect = enum {
    tool_brush,
    tool_place,
    tool_select,
    /// The cells under `ground_a` and `ground_b` show another tile in the
    /// engine, as one undo step, and the engine agrees with the map.
    painted,
    /// One more object, selected: the placer's.
    placed,
    nothing_selected,
    /// The map's object under `target_pick` is selected.
    target_selected,
    /// The placed object, found by a click where it was put, is selected.
    placed_selected,
    /// The placed object turned a sixteenth clockwise, and the engine agrees.
    rotated,
    /// The placed object has left where it was put, is still selected, a
    /// click at the drag's end finds it, and the engine agrees.
    moved,
    /// The placed object is gone, and the engine agrees.
    deleted,
    /// Nothing left to undo: the count, the target's pose and the two cells
    /// as the map had them, the placed object gone, the document clean, and
    /// the engine agreeing with it.
    all_undone,
    saved,
    /// The saved map, opened again, has the original's object count.
    reopened,
    /// The camera moved from where the step started by what the step's
    /// wheel events sum to (view_math.wheelPan), every event the same way.
    panned,
    /// The camera is where the step started: the step's wheel was not the
    /// view's.
    camera_unchanged,
    /// ImGui wants the mouse (the pointer is over a panel), within
    /// `max_wait_frames` frames; the camera has not moved meanwhile.
    panel_has_pointer,
    /// The unsaved-changes prompt is asking (D-23): the map is still dirty
    /// and the loop is still running - closing the window did not discard
    /// anything.
    unsaved_prompt_open,
    /// Cancel resolved the prompt without discarding the map: not asking
    /// any more, still dirty, still at the same path.
    prompt_cancelled,
    /// Save on a shipped map redirected to Save As (D-18) instead of
    /// writing the shipped file: the dialog slot is waiting for one.
    save_became_save_as,
    /// A Shift+wheel step zoomed in (D-10): zoom_steps rose, and the world
    /// point under the wheel's own screen position stayed put (D-14).
    zoomed_at_pointer,
    /// Home reset the zoom to 0 (D-13).
    view_reset,
    /// "Add at view centre" added one sound through the bridge's own list
    /// (T-03-10-01/02's own ABI), as one undo step.
    sound_added,
    /// Ctrl+Z undid the add: the sound list is back to what the map had.
    sound_removed,
};

pub const Step = struct {
    name: []const u8,
    inputs: []const Input,
    expect: Expect,
};

// Measured on coldwinter by resolving a 20-pixel grid around the centre:
// the band 100-180 pixels above it has no object from 200 left to 160
// right, and the poplar with link ID 101 answers every point from 80 to
// 200 below it and 60 left of it to the centre.
const ground_a: Pos = .{ .dx = -120, .dy = -160 };
const ground_between: Pos = .{ .dx = -95, .dy = -160 };
const ground_b: Pos = .{ .dx = -70, .dy = -160 };
const place_at: Pos = .{ .dx = -40, .dy = -120 };
/// Where a click finds the placed object: above its foot, as a sprite's hit
/// box rises from it (the engine tier's PICK_RISE).
const placed_pick: Pos = .{ .dx = -40, .dy = -132 };
const drag_via: Pos = .{ .dx = -20, .dy = -132 };
const drag_to: Pos = .{ .dx = 0, .dy = -132 };
/// Ground with no object on it, for a click that selects nothing.
const empty_ground: Pos = .{ .dx = 100, .dy = -140 };
/// An object of the map, editable (not one of several sharing a link ID).
const target_pick: Pos = .{ .dx = -20, .dy = 140 };
/// Inside the left panel, which ends 280 pixels from the window's left edge
/// on every screen: an absolute x, unlike the `Pos` offsets from the centre.
const left_panel_x: f32 = 100;

fn plain(key: sdl.SDL_Keycode, scancode: sdl.SDL_Scancode) Input {
    return .{ .key = .{ .key = key, .scancode = scancode } };
}

/// Ctrl+Z; view.zig takes Ctrl or Cmd on every platform.
const undo_key: Input = .{ .key = .{ .key = sdl.SDLK_Z, .scancode = sdl.SDL_SCANCODE_Z, .mod = sdl.SDL_KMOD_LCTRL } };

pub const script = [_]Step{
    .{ .name = "key 2 chooses the brush", .inputs = &.{plain(sdl.SDLK_2, sdl.SDL_SCANCODE_2)}, .expect = .tool_brush },
    .{ .name = "a brush stroke paints", .inputs = &.{ .{ .press = ground_a }, .{ .drag = ground_between }, .{ .drag = ground_b }, .{ .release = ground_b } }, .expect = .painted },
    .{ .name = "key 3 chooses the placer", .inputs = &.{plain(sdl.SDLK_3, sdl.SDL_SCANCODE_3)}, .expect = .tool_place },
    .{ .name = "a click places an object", .inputs = &.{ .{ .press = place_at }, .{ .release = place_at } }, .expect = .placed },
    .{ .name = "key 1 chooses the selector, the new object still selected", .inputs = &.{plain(sdl.SDLK_1, sdl.SDL_SCANCODE_1)}, .expect = .tool_select },
    .{ .name = "a click on bare ground selects nothing", .inputs = &.{ .{ .press = empty_ground }, .{ .release = empty_ground } }, .expect = .nothing_selected },
    .{ .name = "a click on an object of the map selects it", .inputs = &.{ .{ .press = target_pick }, .{ .release = target_pick } }, .expect = .target_selected },
    .{ .name = "a click on the placed object selects it", .inputs = &.{ .{ .press = placed_pick }, .{ .release = placed_pick } }, .expect = .placed_selected },
    .{ .name = "E turns it", .inputs = &.{plain(sdl.SDLK_E, sdl.SDL_SCANCODE_E)}, .expect = .rotated },
    .{ .name = "a drag moves it", .inputs = &.{ .{ .press = placed_pick }, .{ .drag = drag_via }, .{ .drag = drag_to }, .{ .release = drag_to } }, .expect = .moved },
    .{ .name = "Delete deletes it", .inputs = &.{plain(sdl.SDLK_DELETE, sdl.SDL_SCANCODE_DELETE)}, .expect = .deleted },
    // Paint, place, turn, move, delete: five edits, five undos.
    .{ .name = "Ctrl+Z undoes all of it", .inputs = &.{ undo_key, undo_key, undo_key, undo_key, undo_key }, .expect = .all_undone },
    // D-18: the smoke's own map is opened by its shipped relative path
    // ("Data\..."), so Save on it must redirect to Save As rather than
    // overwrite the shipped file.
    .{ .name = "Save on a shipped map becomes Save As", .inputs = &.{.save_requested}, .expect = .save_became_save_as },
    .{ .name = "Save As writes the map", .inputs = &.{.save_as}, .expect = .saved },
    .{ .name = "the saved map opens again", .inputs = &.{.open_saved}, .expect = .reopened },
    // The Sounds panel (Task 3): "Add at view centre" adds a sound as one
    // undo step; Ctrl+Z removes it, back to what the reopened map had.
    .{ .name = "Add at view centre adds a sound", .inputs = &.{.add_sound_at_view_centre}, .expect = .sound_added },
    .{ .name = "Ctrl+Z removes it", .inputs = &.{undo_key}, .expect = .sound_removed },
    // D-23: the unsaved-changes prompt on a window close, and Cancel.
    .{ .name = "key 2 chooses the brush again", .inputs = &.{plain(sdl.SDLK_2, sdl.SDL_SCANCODE_2)}, .expect = .tool_brush },
    .{ .name = "a brush stroke makes the map dirty", .inputs = &.{ .{ .press = ground_a }, .{ .drag = ground_between }, .{ .drag = ground_b }, .{ .release = ground_b } }, .expect = .painted },
    .{ .name = "closing the window asks", .inputs = &.{.window_close}, .expect = .unsaved_prompt_open },
    .{ .name = "Cancel keeps it", .inputs = &.{.{ .answer = .cancel }}, .expect = .prompt_cancelled },
    .{ .name = "Ctrl+Z undoes the stroke", .inputs = &.{undo_key}, .expect = .all_undone },
    // Task 7.3: a two-finger swipe, as SDL delivers it on macOS - many
    // small fractional deltas on both axes - pans the map; the same swipe
    // back pans it back; a flipped (natural scrolling) swipe pans by the
    // sign SDL delivered; a wheel over a panel leaves the map alone.
    .{ .name = "a swipe pans the map", .inputs = &.{.{ .wheel = .{ .at = empty_ground, .x = 0.15, .y = 0.35, .count = 12 } }}, .expect = .panned },
    .{ .name = "the swipe back pans it back", .inputs = &.{.{ .wheel = .{ .at = empty_ground, .x = -0.15, .y = -0.35, .count = 12 } }}, .expect = .panned },
    .{ .name = "a natural-scrolling swipe pans by SDL's sign", .inputs = &.{.{ .wheel = .{ .at = empty_ground, .x = -0.2, .y = 0.1, .count = 6, .flipped = true } }}, .expect = .panned },
    // Plan 6, D-10/D-13/D-14: run right after the swipe-pan steps above, so
    // every step before this ran unzoomed and the pointer is still over the
    // map (not stuck over the left panel, whose WantCaptureMouse lag the
    // steps below this comment work around) - Shift+wheel zooms in at the
    // pointer, a plain swipe at that zoom still follows the fingers 1:1, and
    // Home resets the zoom.
    .{ .name = "Shift + wheel zooms in at the pointer", .inputs = &.{.{ .wheel = .{ .at = empty_ground, .x = 0, .y = 1, .count = 1, .mods = sdl.SDL_KMOD_LSHIFT } }}, .expect = .zoomed_at_pointer },
    .{ .name = "a swipe at zoom follows the fingers", .inputs = &.{.{ .wheel = .{ .at = empty_ground, .x = 0.15, .y = 0.35, .count = 12 } }}, .expect = .panned },
    .{ .name = "Home resets the view", .inputs = &.{plain(sdl.SDLK_HOME, sdl.SDL_SCANCODE_HOME)}, .expect = .view_reset },
    // ImGui decides WantCaptureMouse in a later frame from where the
    // pointer is (its input queue trickles one kind of event per frame, and
    // the steps before left a backlog), so the pointer rests on the panel
    // until ImGui has it, as a hand's would before it swipes.
    .{ .name = "the pointer rests on the left panel", .inputs = &.{.{ .wheel = .{ .over_left_panel = true, .x = 0, .y = 0, .count = 0 } }}, .expect = .panel_has_pointer },
    .{ .name = "a wheel over a panel leaves the map alone", .inputs = &.{.{ .wheel = .{ .over_left_panel = true, .x = 0, .y = -1, .count = 3 } }}, .expect = .camera_unchanged },
};

/// Frames drawn before the first step, so the panels have been laid out,
/// ImGui's capture flags describe them, and the bridge has a drawn frame to
/// resolve screen points against.
const settle_frames = 2;

/// How long a `panel_has_pointer` step may wait for ImGui.
const max_wait_frames = 60;

/// The sibling `<dir>/<stem>.~save<ext>` a safe save writes to before the
/// swap (core/files.zig's tempPathFor, plan 6's D-19) - null when `path` does
/// not fit `buffer` or has no extension to preserve.
fn tempSiblingPath(buffer: []u8, path: []const u8) ?[]const u8 {
    const ext = std.fs.path.extension(path);
    const dir = std.fs.path.dirname(path) orelse ".";
    const base = std.fs.path.basename(path);
    const stem = base[0 .. base.len - ext.len];
    return std.fmt.bufPrint(buffer, "{s}{c}{s}.~save{s}", .{ dir, std.fs.path.sep, stem, ext }) catch null;
}

pub const Script = struct {
    editor: *Editor,
    view: *View,
    real: *RealBridge,
    state: *panels.State,
    window_id: sdl.SDL_WindowID,
    save_path: []const u8,

    centre_x: f32 = 0,
    centre_y: f32 = 0,
    frame: usize = 0,
    step: usize = 0,
    passed: bool = false,
    /// A FAIL line has been printed.
    reported: bool = false,

    original_objects: usize,
    /// The map's own sound count, read once at init through the same bridge
    /// call `sound_added`/`sound_removed` check against.
    original_sounds: usize = 0,
    cell_a: [2]i32 = .{ 0, 0 },
    cell_b: [2]i32 = .{ 0, 0 },
    tile_a: u8 = 0,
    tile_b: u8 = 0,
    placed: i32 = -1,
    target: i32 = -1,
    target_pose: Pose = .{ .x = 0, .y = 0, .dir = 0, .player = 0 },
    placed_pose: Pose = .{ .x = 0, .y = 0, .dir = 0, .player = 0 },
    /// The view's camera when the step's inputs were pushed.
    camera_before: [2]f32 = .{ 0, 0 },
    /// The world point the drawn frame showed at the screen's centre then.
    centre_before: ?core.tools.Pointer = null,
    /// The view's zoom_steps when the step's inputs were pushed.
    zoom_steps_before: i32 = 0,
    /// The world point under a wheel step's own `at` position, before its
    /// inputs were pushed - for `zoomed_at_pointer` (D-14), since the zoom
    /// point is not always the screen's centre.
    wheel_point_before: ?core.tools.Pointer = null,
    /// Frames the current step has waited (panel_has_pointer); its inputs
    /// are not pushed again meanwhile.
    waited: usize = 0,

    /// After the map is open and State built.
    pub fn init(editor: *Editor, view: *View, real: *RealBridge, state: *panels.State, window: *sdl.SDL_Window, save_path: []const u8) Script {
        var none: [0]core.bridge.SoundRecord = .{};
        var sounds_count: usize = 0;
        _ = editor.bridge.sounds(&none, &sounds_count);
        return .{
            .editor = editor,
            .view = view,
            .real = real,
            .state = state,
            .window_id = sdl.SDL_GetWindowID(window),
            .save_path = save_path,
            .original_objects = editor.document.objects.items.len,
            .original_sounds = sounds_count,
        };
    }

    /// The screen and what is under the script's positions, read once the
    /// settle frames are drawn: the bridge resolves a screen point through
    /// the last frame's camera, so before any frame there is nothing under
    /// it, and a window the desktop was too small for has been resized by
    /// then (1008x681 on the Windows runner).
    fn prepare(self: *Script) bool {
        const size = self.real.screenSize() orelse return self.fail("no screen size", .{});
        self.centre_x = @as(f32, @floatFromInt(size[0])) / 2;
        self.centre_y = @as(f32, @floatFromInt(size[1])) / 2;
        self.cell_a = self.cellAt(ground_a) orelse return self.fail("no map cell under ground_a", .{});
        self.cell_b = self.cellAt(ground_b) orelse return self.fail("no map cell under ground_b", .{});
        self.tile_a = self.engineTile(self.cell_a) orelse return self.fail("the engine has no tile at {any}", .{self.cell_a});
        self.tile_b = self.engineTile(self.cell_b) orelse return self.fail("the engine has no tile at {any}", .{self.cell_b});
        // The brush's tile, as the palette would choose it. The engine
        // shows a tile of the painted one's terrain type, not always that
        // index (CTerrain::SetTile derives it), so the paint must change
        // terrain type to show: 0 and 14 are in every shipped tileset, in
        // different terrain types (c_bridge_test.zig picks them the same way).
        self.view.brush.tile = if (self.tile_a != 0 and self.tile_b != 0) 0 else 14;
        if (std.mem.indexOfScalar(u8, self.state.tiles(), self.view.brush.tile) == null)
            return self.fail("the tileset does not offer tile {d}", .{self.view.brush.tile});
        const empty = self.resolveAt(empty_ground) orelse return self.fail("empty_ground is off the terrain", .{});
        if (empty.object) |link_id| return self.fail("empty_ground has object {d} on it", .{link_id});
        // Nothing of the map where the placed object is put and clicked, so
        // a click there that answers can only answer with it.
        for ([_]Pos{ ground_a, ground_b, place_at, placed_pick }) |pos| {
            const point = self.resolveAt(pos) orelse return self.fail("{any} is off the terrain", .{pos});
            if (point.object) |link_id| return self.fail("{any} has object {d} on it", .{ pos, link_id });
        }
        const target = self.resolveAt(target_pick) orelse return self.fail("target_pick is off the terrain", .{});
        self.target = target.object orelse return self.fail("no object under target_pick", .{});
        const objects = self.editor.document.objects.items;
        const index = self.editor.document.indexOf(self.target) orelse return self.fail("the map has no object {d}", .{self.target});
        if (panels_logic.readOnlyReason(objects, objects[index])) |reason|
            return self.fail("object {d} under target_pick is kept as it is: {s}", .{ self.target, reason });
        const object = objects[index];
        self.target_pose = .{ .x = object.x, .y = object.y, .dir = object.dir, .player = object.player };
        if (self.view.placer.name.len == 0) return self.fail("the placer has no object", .{});
        return true;
    }

    fn screen(self: *const Script, pos: Pos) [2]f32 {
        return .{ self.centre_x + pos.dx, self.centre_y + pos.dy };
    }

    fn resolveAt(self: *Script, pos: Pos) ?core.tools.Pointer {
        const point = self.screen(pos);
        return self.editor.resolve(point[0], point[1]) catch null;
    }

    fn cellAt(self: *Script, pos: Pos) ?[2]i32 {
        return (self.resolveAt(pos) orelse return null).tile;
    }

    fn engineTile(self: *Script, cell: [2]i32) ?u8 {
        var tile: u8 = 0;
        if (c.BkEditorEngineTile(self.real.session, cell[0], cell[1], &tile) != c.BK_EDITOR_OK) return null;
        return tile;
    }

    /// Before the frame's events are polled: the current step's inputs onto
    /// SDL's queue. False when the loop should stop.
    pub fn beforeFrame(self: *Script) bool {
        if (self.frame < settle_frames or self.step >= script.len) return true;
        if (self.frame == settle_frames and !self.prepare()) return false;
        if (self.waited != 0) return true;
        self.camera_before = .{ self.view.camera_x, self.view.camera_y };
        self.centre_before = self.resolveAt(.{ .dx = 0, .dy = 0 });
        self.zoom_steps_before = self.view.zoom_steps;
        const inputs = script[self.step].inputs;
        self.wheel_point_before = if (inputs.len != 0 and inputs[0] == .wheel) self.resolveAt(inputs[0].wheel.at) else null;
        for (script[self.step].inputs) |input| {
            if (!self.deliver(input)) return false;
        }
        return true;
    }

    /// After the frame and the panels' file actions: the step's check.
    /// False when the loop should stop - the script finished or failed.
    pub fn afterFrame(self: *Script) bool {
        defer self.frame += 1;
        // A Shift+wheel step's SDL_SetModState was only meant to be seen by
        // this frame's own wheel processing, which has already happened by
        // the time afterFrame runs; clearing it here (rather than right
        // after pushing, in `deliver`) is what lets it survive to be read at
        // all. Unconditional and harmless on every other frame.
        sdl.SDL_SetModState(0);
        if (self.frame < settle_frames) return true;
        const step = script[self.step];
        if (step.expect == .panel_has_pointer and !view_mod.captureFlags().mouse and self.waited < max_wait_frames) {
            self.waited += 1;
            return true;
        }
        self.waited = 0;
        if (!self.check(step)) return false;
        self.step += 1;
        if (self.step < script.len) return true;
        self.passed = true;
        return false;
    }

    fn deliver(self: *Script, input: Input) bool {
        switch (input) {
            .key => |key| return self.pushKey(key, true) and self.pushKey(key, false),
            .press => |pos| return self.pushMotion(pos, false) and self.pushButton(pos, true),
            .drag => |pos| return self.pushMotion(pos, true),
            .release => |pos| return self.pushMotion(pos, true) and self.pushButton(pos, false),
            .save_as, .open_saved => {
                const kind: panels_logic.DialogKind = if (input == .save_as) .save_as else .open;
                if (!self.state.actions.dialog.request(kind)) return self.fail("the dialog slot was busy", .{});
                self.state.actions.dialog.deliver(self.save_path);
                return true;
            },
            .window_close => {
                var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
                event.window.type = sdl.SDL_EVENT_WINDOW_CLOSE_REQUESTED;
                event.window.windowID = self.window_id;
                return self.push(&event);
            },
            .answer => |choice| {
                self.state.actions.answer_pending = choice;
                return true;
            },
            .save_requested => {
                self.state.actions.save_requested = true;
                return true;
            },
            .add_sound_at_view_centre => {
                panels.addSoundAtViewCentre(self.state);
                return true;
            },
            .wheel => |wheel| {
                const point = if (wheel.over_left_panel) [2]f32{ left_panel_x, self.centre_y } else self.screen(wheel.at);
                if (!self.pushMotionAt(point, false)) return false;
                // Held through the rest of this frame (cleared in
                // afterFrame): pushed key events do not update SDL's own
                // modifier state, which view.zig's handleWheel reads.
                if (wheel.mods != 0) sdl.SDL_SetModState(wheel.mods);
                for (0..wheel.count) |_| {
                    var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
                    event.wheel.type = sdl.SDL_EVENT_MOUSE_WHEEL;
                    event.wheel.windowID = self.window_id;
                    event.wheel.x = wheel.x;
                    event.wheel.y = wheel.y;
                    event.wheel.direction = if (wheel.flipped) sdl.SDL_MOUSEWHEEL_FLIPPED else sdl.SDL_MOUSEWHEEL_NORMAL;
                    event.wheel.mouse_x = point[0];
                    event.wheel.mouse_y = point[1];
                    if (!self.push(&event)) return false;
                }
                return true;
            },
        }
    }

    fn push(self: *Script, event: *sdl.SDL_Event) bool {
        if (sdl.SDL_PushEvent(event)) return true;
        return self.fail("SDL_PushEvent: {s}", .{sdl.SDL_GetError()});
    }

    fn pushMotion(self: *Script, pos: Pos, left_held: bool) bool {
        return self.pushMotionAt(self.screen(pos), left_held);
    }

    fn pushMotionAt(self: *Script, point: [2]f32, left_held: bool) bool {
        var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
        event.motion.type = sdl.SDL_EVENT_MOUSE_MOTION;
        event.motion.windowID = self.window_id;
        event.motion.state = if (left_held) sdl.SDL_BUTTON_LMASK else 0;
        event.motion.x = point[0];
        event.motion.y = point[1];
        return self.push(&event);
    }

    fn pushButton(self: *Script, pos: Pos, down: bool) bool {
        const point = self.screen(pos);
        var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
        event.button.type = if (down) sdl.SDL_EVENT_MOUSE_BUTTON_DOWN else sdl.SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = self.window_id;
        event.button.button = sdl.SDL_BUTTON_LEFT;
        event.button.down = down;
        event.button.clicks = 1;
        event.button.x = point[0];
        event.button.y = point[1];
        return self.push(&event);
    }

    fn pushKey(self: *Script, key: Key, down: bool) bool {
        var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
        event.key.type = if (down) sdl.SDL_EVENT_KEY_DOWN else sdl.SDL_EVENT_KEY_UP;
        event.key.windowID = self.window_id;
        event.key.key = key.key;
        event.key.scancode = key.scancode;
        event.key.mod = key.mod;
        event.key.down = down;
        return self.push(&event);
    }

    fn check(self: *Script, step: Step) bool {
        const editor = self.editor;
        const objects = editor.document.objects.items.len;
        if (self.view.statusLine().len != 0) return self.stepFail(step, "{s}", .{self.view.statusLine()});
        switch (step.expect) {
            .tool_brush => if (self.view.tool != .brush) return self.stepFail(step, "the tool is {s}", .{@tagName(self.view.tool)}),
            .tool_place => if (self.view.tool != .place) return self.stepFail(step, "the tool is {s}", .{@tagName(self.view.tool)}),
            .tool_select => {
                if (self.view.tool != .select) return self.stepFail(step, "the tool is {s}", .{@tagName(self.view.tool)});
                if (editor.selection != self.placed) return self.stepFail(step, "the selection is {?d}, want the new object {d}", .{ editor.selection, self.placed });
            },
            .painted => {
                if (editor.history.undo_stack.items.len != 1) return self.stepFail(step, "{d} edits recorded, want the stroke as one", .{editor.history.undo_stack.items.len});
                for ([_][2]i32{ self.cell_a, self.cell_b }, [_]u8{ self.tile_a, self.tile_b }) |cell, was| {
                    const now = self.engineTile(cell) orelse return self.stepFail(step, "no tile at {any}", .{cell});
                    if (now == was) return self.stepFail(step, "cell {any} still holds tile {d} after painting {d}", .{ cell, now, self.view.brush.tile });
                }
                if (!self.engineAgrees(step)) return false;
            },
            .placed => {
                if (objects != self.original_objects + 1) return self.stepFail(step, "{d} objects, the map had {d}", .{ objects, self.original_objects });
                const last = editor.document.objects.items[objects - 1];
                if (editor.selection != last.link_id) return self.stepFail(step, "the new object {d} is not selected", .{last.link_id});
                self.placed = last.link_id;
                self.placed_pose = .{ .x = last.x, .y = last.y, .dir = last.dir, .player = last.player };
            },
            .nothing_selected => if (editor.selection) |link_id| return self.stepFail(step, "object {d} is selected", .{link_id}),
            .target_selected => if (editor.selection != self.target) return self.stepFail(step, "the selection is {?d}, want {d}", .{ editor.selection, self.target }),
            .placed_selected => if (editor.selection != self.placed) return self.stepFail(step, "the selection is {?d}, want the placed object {d}", .{ editor.selection, self.placed }),
            .rotated => {
                const object = editor.document.find(self.placed) orelse return self.stepFail(step, "the placed object {d} is gone", .{self.placed});
                const want = @mod(self.placed_pose.dir + core.tools.rotate_step, 65536);
                if (object.dir != want) return self.stepFail(step, "direction {d}, want {d}", .{ object.dir, want });
                if (!self.engineAgrees(step)) return false;
            },
            .moved => {
                const object = editor.document.find(self.placed) orelse return self.stepFail(step, "the placed object {d} is gone", .{self.placed});
                if (object.x == self.placed_pose.x and object.y == self.placed_pose.y)
                    return self.stepFail(step, "the placed object is still at {d},{d}", .{ object.x, object.y });
                if (editor.selection != self.placed) return self.stepFail(step, "the selection is {?d}, want {d}", .{ editor.selection, self.placed });
                // It went where the cursor went: a click at the drag's end
                // finds it.
                const there = self.resolveAt(drag_to) orelse return self.stepFail(step, "drag_to is off the terrain", .{});
                if (there.object != self.placed) return self.stepFail(step, "a click at the drag's end finds {?d}, want the placed object {d}", .{ there.object, self.placed });
                if (!self.engineAgrees(step)) return false;
            },
            .deleted => {
                if (editor.document.find(self.placed) != null) return self.stepFail(step, "the placed object {d} is still there", .{self.placed});
                if (objects != self.original_objects) return self.stepFail(step, "{d} objects, want {d} (one placed and deleted)", .{ objects, self.original_objects });
                if (editor.selection != null) return self.stepFail(step, "the deleted object is still selected", .{});
                if (!self.engineAgrees(step)) return false;
            },
            .all_undone => {
                if (editor.history.canUndo()) return self.stepFail(step, "{d} edits are left to undo", .{editor.history.undo_stack.items.len});
                if (objects != self.original_objects) return self.stepFail(step, "{d} objects, the map had {d}", .{ objects, self.original_objects });
                if (editor.document.find(self.placed) != null) return self.stepFail(step, "the placed object is still there", .{});
                const object = editor.document.find(self.target) orelse return self.stepFail(step, "object {d} did not come back", .{self.target});
                const pose: Pose = .{ .x = object.x, .y = object.y, .dir = object.dir, .player = object.player };
                if (!std.meta.eql(pose, self.target_pose)) return self.stepFail(step, "object {d} came back as {any}, the map had {any}", .{ self.target, pose, self.target_pose });
                if (self.engineTile(self.cell_a) != self.tile_a or self.engineTile(self.cell_b) != self.tile_b)
                    return self.stepFail(step, "the painted cells were not put back", .{});
                if (editor.dirty()) return self.stepFail(step, "the document is still dirty", .{});
                if (!self.engineAgrees(step)) return false;
            },
            .saved => {
                if (editor.status().len != 0) return self.stepFail(step, "{s}", .{editor.status()});
                if (!std.mem.eql(u8, panels_logic.baseName(editor.document.path.items), std.fs.path.basename(self.save_path)))
                    return self.stepFail(step, "the document's path is {s}", .{editor.document.path.items});
                if (editor.dirty()) return self.stepFail(step, "the document is dirty after saving", .{});
                var temp_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
                if (tempSiblingPath(&temp_buffer, self.save_path)) |temp_path| {
                    const left_behind = blk: {
                        _ = std.Io.Dir.cwd().statFile(self.state.io, temp_path, .{}) catch break :blk false;
                        break :blk true;
                    };
                    if (left_behind) return self.stepFail(step, "the temporary save file {s} was left behind", .{temp_path});
                }
            },
            .reopened => {
                if (editor.status().len != 0) return self.stepFail(step, "{s}", .{editor.status()});
                if (objects != self.original_objects) return self.stepFail(step, "{d} objects, the map had {d}", .{ objects, self.original_objects });
                if (editor.document.find(self.target) == null) return self.stepFail(step, "object {d} is not in the saved map", .{self.target});
            },
            .panned => {
                const wheel = step.inputs[0].wheel;
                // Divided the same way view.zig's own handleWheel divides a
                // screen pan by scale: at zoom the map must follow the
                // fingers 1:1 on screen, not by panScreen's zoom-1 gain.
                const scale = if (self.view.scale > 0) self.view.scale else 1;
                var want: view_math.Camera = .{ .x = self.camera_before[0], .y = self.camera_before[1] };
                for (0..wheel.count) |_| {
                    const pan = view_math.wheelPan(.{ .x = wheel.x, .y = wheel.y, .flipped = wheel.flipped }, view_math.wheel_sensitivity);
                    want.panScreen(pan.right_px / scale, pan.up_px / scale, self.view.map);
                }
                const moved = @abs(self.view.camera_x - self.camera_before[0]) + @abs(self.view.camera_y - self.camera_before[1]);
                if (moved < 1) return self.stepFail(step, "the camera did not move from {any}", .{self.camera_before});
                if (@abs(self.view.camera_x - want.x) > 0.5 or @abs(self.view.camera_y - want.y) > 0.5)
                    return self.stepFail(step, "the camera went from {any} to {d},{d}, want {d},{d}", .{ self.camera_before, self.view.camera_x, self.view.camera_y, want.x, want.y });
                // The drawn frame moved with the view's camera: the world
                // point at the screen's centre moved by the same amount.
                const before = self.centre_before orelse return self.stepFail(step, "the centre was off the terrain", .{});
                const after = self.resolveAt(.{ .dx = 0, .dy = 0 }) orelse return self.stepFail(step, "the centre is off the terrain", .{});
                const drawn_x = after.world_x - before.world_x;
                const drawn_y = after.world_y - before.world_y;
                if (@abs(drawn_x - (want.x - self.camera_before[0])) > 4 or @abs(drawn_y - (want.y - self.camera_before[1])) > 4)
                    return self.stepFail(step, "the drawn frame's centre moved by {d},{d}, the camera by {d},{d}", .{ drawn_x, drawn_y, want.x - self.camera_before[0], want.y - self.camera_before[1] });
            },
            .panel_has_pointer => {
                if (!view_mod.captureFlags().mouse) return self.stepFail(step, "ImGui does not want the mouse after {d} frames", .{max_wait_frames});
                if (self.view.camera_x != self.camera_before[0] or self.view.camera_y != self.camera_before[1])
                    return self.stepFail(step, "the camera moved from {any} to {d},{d}", .{ self.camera_before, self.view.camera_x, self.view.camera_y });
            },
            .camera_unchanged => {
                if (self.view.camera_x != self.camera_before[0] or self.view.camera_y != self.camera_before[1])
                    return self.stepFail(step, "the camera moved from {any} to {d},{d}", .{ self.camera_before, self.view.camera_x, self.view.camera_y });
            },
            .unsaved_prompt_open => {
                if (!self.state.actions.prompt.isAsking()) return self.stepFail(step, "the prompt is not asking", .{});
                if (!editor.dirty()) return self.stepFail(step, "the document is not dirty while the prompt asks", .{});
            },
            .prompt_cancelled => {
                if (self.state.actions.prompt.isAsking()) return self.stepFail(step, "the prompt is still asking after Cancel", .{});
                if (!editor.dirty()) return self.stepFail(step, "the document is not dirty after Cancel", .{});
                if (!std.mem.eql(u8, panels_logic.baseName(editor.document.path.items), std.fs.path.basename(self.save_path)))
                    return self.stepFail(step, "the path changed to {s}", .{editor.document.path.items});
            },
            .save_became_save_as => {
                if (!self.state.actions.dialog.waiting() or self.state.actions.dialog.kind != .save_as)
                    return self.stepFail(step, "Save did not redirect to Save As on the shipped map", .{});
                // Cancels it and drains the result, freeing the slot for the
                // next step's real Save As - never letting the real dialog
                // (if it ever answers) touch the shipped file.
                self.state.actions.dialog.deliver(null);
                _ = self.state.actions.dialog.take();
            },
            .zoomed_at_pointer => {
                if (self.view.zoom_steps <= self.zoom_steps_before)
                    return self.stepFail(step, "zoom_steps is {d}, want more than {d}", .{ self.view.zoom_steps, self.zoom_steps_before });
                const wheel = step.inputs[0].wheel;
                const before = self.wheel_point_before orelse return self.stepFail(step, "the zoom point was off the terrain before zooming", .{});
                const after = self.resolveAt(wheel.at) orelse return self.stepFail(step, "the zoom point is off the terrain after zooming", .{});
                const moved = @abs(after.world_x - before.world_x) + @abs(after.world_y - before.world_y);
                if (moved > 2) return self.stepFail(step, "the zoom point moved by {d} world units (D-14)", .{moved});
            },
            .view_reset => {
                if (self.view.zoom_steps != 0) return self.stepFail(step, "zoom_steps is {d}, want 0 (D-13)", .{self.view.zoom_steps});
            },
            .sound_added => {
                var count: usize = 0;
                var none: [0]core.bridge.SoundRecord = .{};
                _ = editor.bridge.sounds(&none, &count);
                if (count != self.original_sounds + 1) return self.stepFail(step, "{d} sounds, the map had {d}", .{ count, self.original_sounds });
                if (editor.history.undo_stack.items.len != 1) return self.stepFail(step, "{d} edits recorded, want the add as one", .{editor.history.undo_stack.items.len});
                if (self.state.selected_sound == null) return self.stepFail(step, "the added sound is not selected", .{});
            },
            .sound_removed => {
                var count: usize = 0;
                var none: [0]core.bridge.SoundRecord = .{};
                _ = editor.bridge.sounds(&none, &count);
                if (count != self.original_sounds) return self.stepFail(step, "{d} sounds, want the original {d} back", .{ count, self.original_sounds });
                if (editor.history.canUndo()) return self.stepFail(step, "{d} edits are left to undo", .{editor.history.undo_stack.items.len});
            },
        }
        return true;
    }

    fn engineAgrees(self: *Script, step: Step) bool {
        if (self.real.engineMatches() == .ok) return true;
        return self.stepFail(step, "the engine disagrees with the map: {s}", .{std.mem.span(c.BkEditorLastMessage(self.real.session))});
    }

    fn stepFail(self: *Script, step: Step, comptime format: []const u8, args: anytype) bool {
        std.debug.print("map-editor: smoke FAIL: {s}: " ++ format ++ " (editor status: {s})\n", .{step.name} ++ args ++ .{self.editor.status()});
        self.reported = true;
        return false;
    }

    fn fail(self: *Script, comptime format: []const u8, args: anytype) bool {
        std.debug.print("map-editor: smoke FAIL: " ++ format ++ "\n", args);
        self.reported = true;
        return false;
    }
};
