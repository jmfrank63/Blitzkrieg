//! The map view's parts that need no window, no engine and no SDL headers:
//! the camera's scrolling and clamping, and the mapping from an SDL mouse
//! button to the tool event it starts, drags or ends. Kept apart from
//! view.zig so these run under a plain `zig build test-map-editor-view`,
//! without the engine's libraries or a GPU.
const std = @import("std");

/// A tile's side in world units: fWorldCellSize (Sources/src/Formats/fmtTerrain.h),
/// fCellSizeX (32) * sqrt(2) - the same value main.zig's --check confirmed
/// against the real engine by centring the camera on a shipped map.
pub const world_cell_size: f32 = 32.0 * std.math.sqrt2;

/// World units per second, in each direction held.
pub const scroll_speed: f32 = 1000.0;

pub const Scroll = struct {
    left: bool = false,
    right: bool = false,
    up: bool = false,
    down: bool = false,
};

/// What the camera is clamped to: the map's size in tiles.
pub const MapSize = struct {
    width_tiles: i32 = 0,
    height_tiles: i32 = 0,
};

pub const Camera = struct {
    x: f32 = 0,
    y: f32 = 0,

    /// Arrow keys, WASD and edge-scrolling add up (holding two opposite
    /// directions cancels, same as any other input system); the result is
    /// clamped to the map so the camera never looks past its edge.
    ///
    /// The directions are the screen's: the bridge places the camera the way
    /// the game does (yaw 45, pitch 30), so screen right is world (+1, +1)
    /// and screen up world (-1, +1), both over sqrt(2). Up and down move twice
    /// as far in the world, as CCamera::Update's forward does, because the
    /// pitch halves how far a world step goes up the screen.
    pub fn scroll(self: *Camera, dir: Scroll, dt_seconds: f32, map: MapSize) void {
        const delta = scroll_speed * dt_seconds / std.math.sqrt2;
        var right: f32 = 0;
        var up: f32 = 0;
        if (dir.left) right -= delta;
        if (dir.right) right += delta;
        if (dir.up) up += 2 * delta;
        if (dir.down) up -= 2 * delta;
        self.x += right - up;
        self.y += right + up;
        self.clamp(map);
    }

    /// Moves the camera by a distance on the screen: `right_px` to the
    /// right, `up_px` up. The same screen-to-world mapping as `scroll`:
    /// one pixel is one world unit along screen right, and two along screen
    /// up. Nothing is rounded, so many small moves add up to exactly the
    /// one large move they sum to.
    pub fn panScreen(self: *Camera, right_px: f32, up_px: f32, map: MapSize) void {
        const right = right_px / std.math.sqrt2;
        const up = 2 * up_px / std.math.sqrt2;
        self.x += right - up;
        self.y += right + up;
        self.clamp(map);
    }

    pub fn clamp(self: *Camera, map: MapSize) void {
        const max_x = @as(f32, @floatFromInt(map.width_tiles)) * world_cell_size;
        const max_y = @as(f32, @floatFromInt(map.height_tiles)) * world_cell_size;
        self.x = std.math.clamp(self.x, 0, max_x);
        self.y = std.math.clamp(self.y, 0, max_y);
    }
};

/// Screen pixels the map pans per unit of an SDL wheel event. On macOS a
/// trackpad's unit is 10 points of finger travel (SDL multiplies the precise
/// deltas by 0.1, SDL_cocoamouse.m), so 20 moves the map twice as far as the
/// fingers went. A mouse wheel's notch is about one unit, so a notch pans
/// 20 pixels, more when macOS accelerates a fast spin. SDL has no field that
/// tells the two apart; one gain serves both.
pub const wheel_pixels_per_unit: f32 = 20.0;

/// The player's wheel/trackpad sensitivity, a multiplier on
/// `wheel_pixels_per_unit`: 1 is the gain above. The game has it as an
/// option (GamePlay.TrackpadScroll, 0.25x-4x); the editor has no settings
/// store yet.
/// TODO(plan 6, editor settings): make this the editor's setting.
pub const wheel_sensitivity: f32 = 1.0;

/// An SDL_MouseWheelEvent's deltas, as view.zig hands them over. `flipped`
/// is SDL_MOUSEWHEEL_FLIPPED (natural scrolling).
pub const WheelEvent = struct { x: f32, y: f32, flipped: bool = false };

/// A pan on the screen, in pixels: right and up.
pub const ScreenPan = struct { right_px: f32, up_px: f32 };

/// The pan one wheel event asks for. SDL's x is positive to the right and
/// its y positive away from the user. Both already carry the user's
/// natural-scrolling setting: macOS inverts scrollingDelta itself, and
/// `direction` only reports that it did. So `flipped` is deliberately not
/// applied again. Applying it would scroll the map against the fingers.
/// Fractions pass through unrounded: a slow swipe's 0.1s add up; they are
/// not truncated away or rounded into +1/-1 steps.
pub fn wheelPan(event: WheelEvent, sensitivity: f32) ScreenPan {
    const gain = wheel_pixels_per_unit * sensitivity;
    return .{ .right_px = event.x * gain, .up_px = event.y * gain };
}

/// Whether two nonzero deltas point the same way - no `std.math.sign` in
/// this Zig; a plain comparison is enough for the residual-reversal check.
fn sameDirection(a: f32, b: f32) bool {
    return (a > 0 and b > 0) or (a < 0 and b < 0);
}

/// The whole steps a residual has crossed, snapping to the nearest integer
/// first when it is within floating-point noise of one: `log(1.2, 1/1.2)` is
/// exactly -1 in real numbers but `-0.999999...` in f32, and a plain
/// `trunc` would silently drop that step.
fn wholeSteps(residual: f32) i32 {
    const rounded = @round(residual);
    if (@abs(residual - rounded) < 0.0005) return @intFromFloat(rounded);
    return @intFromFloat(std.math.trunc(residual));
}

/// Shift+wheel/swipe or a trackpad pinch, folded into whole zoom steps: the
/// game's own zoom (NSceneScreenScale) moves in integer steps, and a
/// fractional trackpad delta or pinch scale has to accumulate toward the
/// next whole one rather than being rounded or dropped.
pub const ZoomWheel = struct {
    residual: f32 = 0,

    /// One wheel/swipe delta in, the whole steps it crossed out (0, one, or
    /// more for a fast spin); the leftover fraction is carried to the next
    /// `feed`. A delta that reverses direction from the carried fraction
    /// drops it first: the residual is "how far the finger has moved this
    /// way", and it means nothing once the finger reverses.
    pub fn feed(self: *ZoomWheel, delta: f32) i32 {
        if (delta == 0) return 0;
        if (self.residual != 0 and !sameDirection(self.residual, delta)) self.residual = 0;
        self.residual += delta;
        const steps = wholeSteps(self.residual);
        self.residual -= @floatFromInt(steps);
        return steps;
    }
};

/// A wheel event's zoom delta: y, or x when y is 0 - macOS turns Shift + a
/// mouse wheel's vertical notches into a horizontal scroll (the same reason
/// `wheelPan` takes both axes for a plain pan).
pub fn zoomDelta(x: f32, y: f32) f32 {
    return if (y != 0) y else x;
}

/// `GFX.World.ZoomFactor`'s default (Scene/SceneScreenScale.h,
/// `NSceneScreenScale::GetZoomStepFactor`): each zoom step scales the view by
/// this factor, so a pinch's scale is folded into steps on a log of this base.
pub const pinch_zoom_step_factor: f32 = 1.2;

/// A trackpad pinch, folded into whole zoom steps the same way `ZoomWheel`
/// folds a wheel/swipe delta - but on a log scale, since SDL's pinch `scale`
/// is multiplicative (scale < 1 zooms out, > 1 zooms in) rather than additive.
pub const PinchZoom = struct {
    log_residual: f32 = 0,

    /// The pinch update's `scale` (since the last update) in, the whole
    /// zoom steps it crossed out. A non-positive scale (should not happen,
    /// SDL's own doc gives no bound) folds to no steps and leaves the
    /// residual alone rather than feeding `log` a domain error.
    pub fn feed(self: *PinchZoom, scale: f32) i32 {
        if (scale <= 0) return 0;
        self.log_residual += std.math.log(f32, pinch_zoom_step_factor, scale);
        const steps = wholeSteps(self.log_residual);
        self.log_residual -= @floatFromInt(steps);
        return steps;
    }

    /// Begin/end of a pinch gesture: no fraction should carry from one
    /// gesture into the next.
    pub fn reset(self: *PinchZoom) void {
        self.log_residual = 0;
    }
};

test "ZoomWheel: a mouse wheel's notches give one step each" {
    var zoom: ZoomWheel = .{};
    try std.testing.expectEqual(@as(i32, 1), zoom.feed(1.0));
    try std.testing.expectEqual(@as(i32, 1), zoom.feed(1.0));
    try std.testing.expectEqual(@as(i32, -1), zoom.feed(-1.0));
}

test "ZoomWheel: four trackpad deltas of 0.3 give one step, on the fourth" {
    var zoom: ZoomWheel = .{};
    try std.testing.expectEqual(@as(i32, 0), zoom.feed(0.3));
    try std.testing.expectEqual(@as(i32, 0), zoom.feed(0.3));
    try std.testing.expectEqual(@as(i32, 0), zoom.feed(0.3));
    try std.testing.expectEqual(@as(i32, 1), zoom.feed(0.3));
}

test "ZoomWheel: a reversal drops the carried residual" {
    var zoom: ZoomWheel = .{};
    _ = zoom.feed(0.9);
    try std.testing.expectEqual(@as(i32, 0), zoom.feed(-0.3));
    try std.testing.expectApproxEqAbs(@as(f32, -0.3), zoom.residual, 0.0001);
}

test "zoomDelta: y wins when nonzero, x only when y is 0" {
    try std.testing.expectEqual(@as(f32, 2), zoomDelta(1, 2));
    try std.testing.expectEqual(@as(f32, 1), zoomDelta(1, 0));
    try std.testing.expectEqual(@as(f32, 0), zoomDelta(0, 0));
}

test "PinchZoom: scale steps of exactly the zoom factor give one step each way" {
    var zoom: PinchZoom = .{};
    try std.testing.expectEqual(@as(i32, 1), zoom.feed(pinch_zoom_step_factor));
    try std.testing.expectEqual(@as(i32, -1), zoom.feed(1 / pinch_zoom_step_factor));
}

test "PinchZoom: small scale updates accumulate toward the next step" {
    var zoom: PinchZoom = .{};
    var steps: i32 = 0;
    for (0..10) |_| steps += zoom.feed(1.02);
    try std.testing.expect(steps >= 1);
}

test "PinchZoom: reset clears a carried residual between gestures" {
    var zoom: PinchZoom = .{};
    _ = zoom.feed(1.1);
    try std.testing.expect(zoom.log_residual != 0);
    zoom.reset();
    try std.testing.expectEqual(@as(f32, 0), zoom.log_residual);
}

const test_map: MapSize = .{ .width_tiles = 96, .height_tiles = 96 };

fn applyWheel(camera: *Camera, event: WheelEvent) void {
    const pan = wheelPan(event, wheel_sensitivity);
    camera.panScreen(pan.right_px, pan.up_px, test_map);
}

test "a wheel's y pans the map up and down the screen, its x across it" {
    var camera: Camera = .{ .x = 2000, .y = 2000 };
    applyWheel(&camera, .{ .x = 0, .y = 1 });
    // Screen up is world (-1, +1).
    try std.testing.expect(camera.x < 2000 and camera.y > 2000);
    camera = .{ .x = 2000, .y = 2000 };
    applyWheel(&camera, .{ .x = 1, .y = 0 });
    // Screen right is world (+1, +1).
    try std.testing.expect(camera.x > 2000 and camera.y > 2000);
    try std.testing.expectApproxEqAbs(camera.x, camera.y, 0.001);
    camera = .{ .x = 2000, .y = 2000 };
    applyWheel(&camera, .{ .x = -1, .y = -1 });
    try std.testing.expect(camera.y < 2000);
}

test "a slow swipe's small fractional deltas add up to the large one, moving the same way every event" {
    var small: Camera = .{ .x = 2000, .y = 2000 };
    var previous_y = small.y;
    for (0..30) |_| {
        applyWheel(&small, .{ .x = 0, .y = 0.1 });
        // Monotonic: every same-sign event moves on, none steps back.
        try std.testing.expect(small.y > previous_y);
        previous_y = small.y;
    }
    var large: Camera = .{ .x = 2000, .y = 2000 };
    applyWheel(&large, .{ .x = 0, .y = 3.0 });
    try std.testing.expectApproxEqAbs(large.x, small.x, 0.01);
    try std.testing.expectApproxEqAbs(large.y, small.y, 0.01);
}

test "mixed-sign micro-jitter nets out, with no step larger than the jitter itself" {
    var camera: Camera = .{ .x = 2000, .y = 2000 };
    const jitter = [_]f32{ 0.04, -0.03, 0.02, -0.05, 0.03, -0.01 };
    const one_step = wheelPan(.{ .x = 0, .y = 0.05 }, wheel_sensitivity).up_px * 2 / std.math.sqrt2;
    for (jitter) |y| {
        const before = camera.y;
        applyWheel(&camera, .{ .x = 0, .y = y });
        try std.testing.expect(@abs(camera.y - before) <= one_step + 0.001);
    }
    // The six sum to zero.
    try std.testing.expectApproxEqAbs(@as(f32, 2000), camera.x, 0.01);
    try std.testing.expectApproxEqAbs(@as(f32, 2000), camera.y, 0.01);
}

test "natural scrolling: a flipped event pans by the sign SDL delivered, not flipped a second time" {
    var natural: Camera = .{ .x = 2000, .y = 2000 };
    var classic: Camera = .{ .x = 2000, .y = 2000 };
    applyWheel(&natural, .{ .x = 0.3, .y = -0.7, .flipped = true });
    applyWheel(&classic, .{ .x = 0.3, .y = -0.7, .flipped = false });
    try std.testing.expectEqual(classic.x, natural.x);
    try std.testing.expectEqual(classic.y, natural.y);
}

test "a mouse wheel's notches pan by whole, equal steps" {
    var camera: Camera = .{ .x = 2000, .y = 2000 };
    applyWheel(&camera, .{ .x = 0, .y = 1 });
    const first = camera.y - 2000;
    applyWheel(&camera, .{ .x = 0, .y = 1 });
    try std.testing.expectApproxEqAbs(2 * first, camera.y - 2000, 0.001);
    try std.testing.expectApproxEqAbs(wheel_pixels_per_unit * 2 / std.math.sqrt2, first, 0.001);
}

test "the sensitivity scales a pan monotonically, and the default leaves it as it was" {
    const event: WheelEvent = .{ .x = 0.3, .y = -0.2 };
    const default_pan = wheelPan(event, wheel_sensitivity);
    try std.testing.expectEqual(event.x * wheel_pixels_per_unit, default_pan.right_px);
    try std.testing.expectEqual(event.y * wheel_pixels_per_unit, default_pan.up_px);
    var previous: f32 = 0;
    for ([_]f32{ 0.25, 0.5, 1, 2, 4 }) |sensitivity| {
        const pan = wheelPan(event, sensitivity);
        try std.testing.expect(pan.right_px > previous);
        try std.testing.expectApproxEqAbs(default_pan.right_px * sensitivity, pan.right_px, 0.0001);
        previous = pan.right_px;
    }
}

test "a wheel pan clamps at the map's edge" {
    var camera: Camera = .{ .x = 10, .y = 10 };
    applyWheel(&camera, .{ .x = -100, .y = 0 });
    try std.testing.expectEqual(@as(f32, 0), camera.x);
    try std.testing.expectEqual(@as(f32, 0), camera.y);
}

/// SDL_BUTTON_LEFT/SDL_BUTTON_RIGHT (SDL_mouse.h): fixed by SDL's own ABI,
/// so naming them here does not need the sdl3 module.
pub const sdl_button_left: u8 = 1;
pub const sdl_button_right: u8 = 3;

/// The two ends of a mouse-button press, as SDL_MouseButtonEvent reports
/// them (view.zig hands over event.button and event.down, not the whole
/// SDL_Event).
pub const ButtonEvent = struct { button: u8, down: bool };

pub const EventKind = enum { press, release };

/// Only the left button drives an edit; any other button is not a tool
/// event at all (the middle button pans the camera in view.zig, and the
/// right button does nothing yet).
pub fn kindOf(event: ButtonEvent) ?EventKind {
    if (event.button != sdl_button_left) return null;
    return if (event.down) .press else .release;
}

test "scroll speed: edge and keys add up, and clamp at the map" {
    var camera: Camera = .{ .x = 100, .y = 100 };
    camera.scroll(.{ .left = true }, 0.5, .{ .width_tiles = 96, .height_tiles = 96 });
    try std.testing.expect(camera.x < 100 and camera.y < 100);
    camera = .{ .x = 0, .y = 0 };
    camera.scroll(.{ .left = true }, 10, .{ .width_tiles = 96, .height_tiles = 96 });
    try std.testing.expectEqual(@as(f32, 0), camera.x);
    try std.testing.expectEqual(@as(f32, 0), camera.y);
}

test "scroll follows the screen of the game's camera: up is world (-1, +1), at twice the step" {
    const map: MapSize = .{ .width_tiles = 96, .height_tiles = 96 };
    var camera: Camera = .{ .x = 2000, .y = 2000 };
    camera.scroll(.{ .up = true }, 0.1, map);
    const step = scroll_speed * 0.1 / std.math.sqrt2;
    try std.testing.expectApproxEqAbs(@as(f32, 2000) - 2 * step, camera.x, 0.01);
    try std.testing.expectApproxEqAbs(@as(f32, 2000) + 2 * step, camera.y, 0.01);
    camera = .{ .x = 2000, .y = 2000 };
    camera.scroll(.{ .right = true }, 0.1, map);
    try std.testing.expectApproxEqAbs(@as(f32, 2000) + step, camera.x, 0.01);
    try std.testing.expectApproxEqAbs(@as(f32, 2000) + step, camera.y, 0.01);
    camera = .{ .x = 2000, .y = 2000 };
    camera.scroll(.{ .up = true, .down = true, .left = true, .right = true }, 0.1, map);
    try std.testing.expectEqual(@as(f32, 2000), camera.x);
    try std.testing.expectEqual(@as(f32, 2000), camera.y);
}

const sdl_left_down: ButtonEvent = .{ .button = sdl_button_left, .down = true };
const sdl_right_down: ButtonEvent = .{ .button = sdl_button_right, .down = true };

test "a mouse button maps to a tool event, and only the left button edits" {
    try std.testing.expectEqual(EventKind.press, kindOf(sdl_left_down).?);
    try std.testing.expect(kindOf(sdl_right_down) == null);
}

test "scroll clamps at the far edge too" {
    var camera: Camera = .{ .x = 3000, .y = 3000 };
    camera.scroll(.{ .right = true }, 10, .{ .width_tiles = 96, .height_tiles = 96 });
    const max = @as(f32, 96) * world_cell_size;
    try std.testing.expectEqual(max, camera.x);
    try std.testing.expectEqual(max, camera.y);
}

test "a release maps to a tool event too" {
    try std.testing.expectEqual(EventKind.release, kindOf(.{ .button = sdl_button_left, .down = false }).?);
}

/// What an SDL event is, for routing: `host.handleEvent`'s bool return is
/// `ImGui_ImplSDL3_ProcessEvent`'s, which is true for every mouse/keyboard
/// event on our window (it means "processed", not "wanted") - it cannot be
/// used to decide whether the view should also see the event. This is a
/// coarser classification than `EventKind`/`kindOf` above: it only tells
/// `shouldDeliver` which of ImGui's two capture flags applies, not which
/// tool event (if any) the event becomes.
pub const InputEventKind = enum { mouse_button, mouse_motion, mouse_wheel, key, other };

/// ImGui's own idea of who wants an event, read from `igGetIO()` after
/// `host.handleEvent` has processed it.
pub const Capture = struct {
    mouse: bool = false,
    keyboard: bool = false,
};

/// Whether the view should also see an event `host.handleEvent` already
/// processed. A mouse event is delivered when ImGui does not want the mouse,
/// or when the view already has an open gesture (a press it saw reach it) -
/// so a drag or release that strays over a panel mid-gesture still reaches
/// the tool that owns it, rather than leaving a Selector or Brush gesture
/// open forever. A key event is delivered when ImGui does not want the
/// keyboard. Anything else (window events, quit, ...) always reaches the
/// view; main.zig handles quit itself before this even runs.
pub fn shouldDeliver(kind: InputEventKind, capture: Capture, gesture_active: bool) bool {
    return switch (kind) {
        .mouse_button, .mouse_motion => gesture_active or !capture.mouse,
        // A wheel is not part of a gesture: over a panel it is the panel's
        // (ImGui scrolls it) even mid-drag, or one swipe would scroll both.
        .mouse_wheel => !capture.mouse,
        .key => !capture.keyboard,
        .other => true,
    };
}

test "routing: ImGui's capture flags gate mouse and key events, but an open gesture overrides the mouse ones" {
    const busy: Capture = .{ .mouse = true, .keyboard = true };
    const free: Capture = .{ .mouse = false, .keyboard = false };
    try std.testing.expect(!shouldDeliver(.mouse_button, busy, false));
    try std.testing.expect(shouldDeliver(.mouse_button, busy, true)); // the release of a gesture the view started
    try std.testing.expect(shouldDeliver(.mouse_motion, busy, true));
    try std.testing.expect(shouldDeliver(.mouse_button, free, false));
    try std.testing.expect(!shouldDeliver(.key, busy, false));
    try std.testing.expect(shouldDeliver(.key, free, false));
    try std.testing.expect(shouldDeliver(.other, busy, false));
}

test "routing: a wheel over a panel is the panel's, even during a gesture; over the map it is the view's" {
    const busy: Capture = .{ .mouse = true, .keyboard = true };
    const free: Capture = .{ .mouse = false, .keyboard = false };
    try std.testing.expect(!shouldDeliver(.mouse_wheel, busy, false));
    try std.testing.expect(!shouldDeliver(.mouse_wheel, busy, true));
    try std.testing.expect(shouldDeliver(.mouse_wheel, free, false));
    try std.testing.expect(shouldDeliver(.mouse_wheel, free, true));
}
