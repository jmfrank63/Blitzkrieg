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

/// Who put the view's status message up. Most messages are `general`: the
/// next edit that succeeds (or is merely refused) clears them, whoever set
/// them. A message from an operation that can later succeed on its own,
/// with no edit in between, is tagged, so that success clears it: a Test in
/// game that failed and then launched (WINDOWS.md 1), a frame that failed
/// and then presented, a dialog that was busy and has since ended.
pub const StatusSource = enum { general, test_launch, frame, dialog };

/// The view's part of the status bar: one message, and the source that set
/// it. `clearFrom` erases the message only if that source set it, so one
/// operation's success never wipes another's failure.
pub const StatusSlot = struct {
    buffer: [512]u8 = undefined,
    len: usize = 0,
    source: StatusSource = .general,

    pub fn line(self: *const StatusSlot) []const u8 {
        return self.buffer[0..self.len];
    }

    /// `prefix` then `message`, cut to the buffer. Replaces whatever was
    /// there, from any source.
    pub fn set(self: *StatusSlot, source: StatusSource, prefix: []const u8, message: []const u8) void {
        const prefix_len = @min(prefix.len, self.buffer.len);
        @memcpy(self.buffer[0..prefix_len], prefix[0..prefix_len]);
        const message_len = @min(message.len, self.buffer.len - prefix_len);
        @memcpy(self.buffer[prefix_len..][0..message_len], message[0..message_len]);
        self.len = prefix_len + message_len;
        self.source = source;
    }

    /// Clears the message, whoever set it.
    pub fn clear(self: *StatusSlot) void {
        self.len = 0;
        self.source = .general;
    }

    /// Clears the message only if `source` set it.
    pub fn clearFrom(self: *StatusSlot, source: StatusSource) void {
        if (self.source == source) self.clear();
    }
};

test "StatusSlot: a source clears its own message, never another's" {
    var slot: StatusSlot = .{};
    slot.set(.test_launch, "test in game: ", "the copy would not save");
    try std.testing.expectEqualStrings("test in game: the copy would not save", slot.line());
    slot.clearFrom(.frame);
    try std.testing.expectEqualStrings("test in game: the copy would not save", slot.line());
    slot.clearFrom(.test_launch);
    try std.testing.expectEqualStrings("", slot.line());

    slot.set(.general, "autosave failed: ", "disk full");
    slot.clearFrom(.test_launch);
    try std.testing.expectEqualStrings("autosave failed: disk full", slot.line());
    slot.clear();
    try std.testing.expectEqualStrings("", slot.line());
}

test "StatusSlot: a later message from another source takes the slot over" {
    var slot: StatusSlot = .{};
    slot.set(.test_launch, "test in game: ", "no test path");
    slot.set(.frame, "failed: ", "DeviceLost");
    slot.clearFrom(.test_launch);
    try std.testing.expectEqualStrings("failed: DeviceLost", slot.line());
    slot.clearFrom(.frame);
    try std.testing.expectEqualStrings("", slot.line());
}

test "StatusSlot: an over-long message is cut to the buffer, never overrun" {
    var slot: StatusSlot = .{};
    const long: [600]u8 = @splat('x');
    slot.set(.general, "failed: ", &long);
    try std.testing.expectEqual(slot.buffer.len, slot.line().len);
    try std.testing.expectEqualStrings("failed: ", slot.line()[0..8]);
}

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
/// option (GamePlay.TrackpadScroll, 0.25x-4x); this is only the documented
/// default for a fresh `View.wheel_sensitivity` field (plan 6) - the app
/// overrides it from `core.settings.Settings.scroll_speed` once the
/// settings file loads, or the Settings window changes it (D-25).
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

test "a wheel notch up pans the map 20 px toward the screen's top: 28.28 world units along (-1, +1)" {
    // Literal positions (WINDOWS.md 3): SDL's y is positive away from the
    // user, a notch is 20 screen px, and screen up is world (-1, +1) at two
    // world units per pixel over sqrt(2); screen right (+1, +1) at one.
    var camera: Camera = .{ .x = 2000, .y = 2000 };
    applyWheel(&camera, .{ .x = 0, .y = 1 });
    try std.testing.expectApproxEqAbs(@as(f32, 1971.716), camera.x, 0.001);
    try std.testing.expectApproxEqAbs(@as(f32, 2028.284), camera.y, 0.001);
    camera = .{ .x = 2000, .y = 2000 };
    applyWheel(&camera, .{ .x = 0, .y = -1 });
    try std.testing.expectApproxEqAbs(@as(f32, 2028.284), camera.x, 0.001);
    try std.testing.expectApproxEqAbs(@as(f32, 1971.716), camera.y, 0.001);
    camera = .{ .x = 2000, .y = 2000 };
    applyWheel(&camera, .{ .x = 1, .y = 0 });
    try std.testing.expectApproxEqAbs(@as(f32, 2014.142), camera.x, 0.001);
    try std.testing.expectApproxEqAbs(@as(f32, 2014.142), camera.y, 0.001);
    camera = .{ .x = 2000, .y = 2000 };
    applyWheel(&camera, .{ .x = -1, .y = -1 });
    try std.testing.expectApproxEqAbs(@as(f32, 2014.142), camera.x, 0.001);
    try std.testing.expectApproxEqAbs(@as(f32, 1957.574), camera.y, 0.001);
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
    try std.testing.expectApproxEqAbs(@as(f32, 28.284), first, 0.001);
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

/// SDL_BUTTON_LEFT/SDL_BUTTON_MIDDLE/SDL_BUTTON_RIGHT (SDL_mouse.h): fixed by
/// SDL's own ABI, so naming them here does not need the sdl3 module.
pub const sdl_button_left: u8 = 1;
pub const sdl_button_middle: u8 = 2;
pub const sdl_button_right: u8 = 3;

/// SDL_BUTTON_MASK(X) (SDL_mouse.h): `1u << (X - 1)`, as SDL_GetMouseState's
/// return value and SDL_MouseMotionEvent.state report it - SDL_BUTTON_LMASK,
/// SDL_BUTTON_MMASK and SDL_BUTTON_RMASK, named here for the same ABI reason
/// as the button numbers above.
pub const sdl_button_lmask: u32 = 1 << (sdl_button_left - 1);
pub const sdl_button_mmask: u32 = 1 << (sdl_button_middle - 1);
pub const sdl_button_rmask: u32 = 1 << (sdl_button_right - 1);

/// The two ends of a mouse-button press, as SDL_MouseButtonEvent reports
/// them (view.zig hands over event.button's fields, not the whole SDL_Event).
/// `clicks` is SDL's own click count: 1 for a single click, 2 for the second
/// of a double click.
/// `wants_double_click` is the active tool's registry entry
/// (`needs_double_click`): a tool that has no use for a double click gets the
/// second click of a fast pair as an ordinary press and release (WR-C03).
pub const ButtonEvent = struct { button: u8, down: bool, clicks: u8 = 1, wants_double_click: bool = true };

pub const EventKind = enum { press, release, right_press, right_release, double_click };

/// Which tool event a button event is, if any. The left button presses and
/// releases; the right button does the same on its own; the middle button
/// pans the camera in view.zig and is no tool event. SDL sends a double click
/// as a press and release with `clicks == 1`, then another pair with `clicks
/// == 2` (Pitfall 13): the second press is a `double_click` on top of the
/// single click that already arrived, and the second release ends nothing (no
/// press was opened for it).
pub fn kindOf(event: ButtonEvent) ?EventKind {
    switch (event.button) {
        sdl_button_left => {
            const double = event.clicks == 2 and event.wants_double_click;
            if (event.down) return if (double) .double_click else .press;
            return if (double) null else .release;
        },
        sdl_button_right => return if (event.down) .right_press else .right_release,
        else => return null,
    }
}

/// A pan or a tool gesture whose button `view.zig`'s own press and release
/// handlers never saw let go: a release ImGui or another window took (Task 2,
/// carried from plan 5: "a middle-button release ImGui takes can leave
/// panning stuck" / "view.zig's gesture handling relies on main.zig's router
/// filtering, with no guard of its own"). Read every frame from
/// `SDL_GetMouseState`'s own button mask, which reports the buttons actually
/// down right now regardless of which window - if any - got the release
/// event. Each button ends only its own gesture.
pub const StaleGesture = struct { end_pan: bool = false, end_left: bool = false, end_right: bool = false };

pub fn staleGesture(buttons_down_mask: u32, panning: bool, left_down: bool, right_down: bool) StaleGesture {
    return .{
        .end_pan = panning and buttons_down_mask & sdl_button_mmask == 0,
        .end_left = left_down and buttons_down_mask & sdl_button_lmask == 0,
        .end_right = right_down and buttons_down_mask & sdl_button_rmask == 0,
    };
}

test "staleGesture: panning ends once the middle button is no longer down, not before" {
    try std.testing.expect(staleGesture(0, true, false, false).end_pan);
    try std.testing.expect(!staleGesture(sdl_button_mmask, true, false, false).end_pan);
    try std.testing.expect(!staleGesture(0, false, false, false).end_pan);
}

test "staleGesture: a left gesture ends once the left button is no longer down, not before" {
    try std.testing.expect(staleGesture(0, false, true, false).end_left);
    try std.testing.expect(!staleGesture(sdl_button_lmask, false, true, false).end_left);
    try std.testing.expect(!staleGesture(0, false, false, false).end_left);
}

test "staleGesture: both can end in the same frame, and the other button's own state never saves one" {
    const both = staleGesture(0, true, true, false);
    try std.testing.expect(both.end_pan);
    try std.testing.expect(both.end_left);
    const left_only = staleGesture(sdl_button_mmask, true, true, false);
    try std.testing.expect(!left_only.end_pan);
    try std.testing.expect(left_only.end_left);
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

test "scroll follows the screen of the game's camera: 0.1 s of up is 141.42 world units along (-1, +1), of right 70.71 along (+1, +1)" {
    // Literal positions, not ones worked out from scroll_speed (WINDOWS.md
    // 3): the game's camera (yaw 45, pitch 30) puts screen up along world
    // (-1, +1) and screen right along (+1, +1) - the engine tier checks the
    // same directions against BkEditorScreenToWorld - and 1000 world units
    // a second, twice that up the screen, is 100 and 200 in 0.1 s.
    const map: MapSize = .{ .width_tiles = 96, .height_tiles = 96 };
    var camera: Camera = .{ .x = 2000, .y = 2000 };
    camera.scroll(.{ .up = true }, 0.1, map);
    try std.testing.expectApproxEqAbs(@as(f32, 1858.579), camera.x, 0.01);
    try std.testing.expectApproxEqAbs(@as(f32, 2141.421), camera.y, 0.01);
    camera = .{ .x = 2000, .y = 2000 };
    camera.scroll(.{ .down = true }, 0.1, map);
    try std.testing.expectApproxEqAbs(@as(f32, 2141.421), camera.x, 0.01);
    try std.testing.expectApproxEqAbs(@as(f32, 1858.579), camera.y, 0.01);
    camera = .{ .x = 2000, .y = 2000 };
    camera.scroll(.{ .right = true }, 0.1, map);
    try std.testing.expectApproxEqAbs(@as(f32, 2070.711), camera.x, 0.01);
    try std.testing.expectApproxEqAbs(@as(f32, 2070.711), camera.y, 0.01);
    camera = .{ .x = 2000, .y = 2000 };
    camera.scroll(.{ .left = true }, 0.1, map);
    try std.testing.expectApproxEqAbs(@as(f32, 1929.289), camera.x, 0.01);
    try std.testing.expectApproxEqAbs(@as(f32, 1929.289), camera.y, 0.01);
    camera = .{ .x = 2000, .y = 2000 };
    camera.scroll(.{ .up = true, .down = true, .left = true, .right = true }, 0.1, map);
    try std.testing.expectEqual(@as(f32, 2000), camera.x);
    try std.testing.expectEqual(@as(f32, 2000), camera.y);
}

const sdl_left_down: ButtonEvent = .{ .button = sdl_button_left, .down = true };
const sdl_right_down: ButtonEvent = .{ .button = sdl_button_right, .down = true };

test "a mouse button maps to a tool event: left and right edit, the middle button does not" {
    try std.testing.expectEqual(EventKind.press, kindOf(sdl_left_down).?);
    try std.testing.expectEqual(EventKind.right_press, kindOf(sdl_right_down).?);
    try std.testing.expectEqual(EventKind.right_release, kindOf(.{ .button = sdl_button_right, .down = false }).?);
    try std.testing.expect(kindOf(.{ .button = sdl_button_middle, .down = true }) == null);
    try std.testing.expect(kindOf(.{ .button = 4, .down = true }) == null);
}

test "kindOf: the second click of a double click is a double_click, its release ends nothing" {
    try std.testing.expectEqual(EventKind.press, kindOf(.{ .button = sdl_button_left, .down = true, .clicks = 1 }).?);
    try std.testing.expectEqual(EventKind.release, kindOf(.{ .button = sdl_button_left, .down = false, .clicks = 1 }).?);
    try std.testing.expectEqual(EventKind.double_click, kindOf(.{ .button = sdl_button_left, .down = true, .clicks = 2 }).?);
    try std.testing.expect(kindOf(.{ .button = sdl_button_left, .down = false, .clicks = 2 }) == null);
    // A third click is an ordinary press again.
    try std.testing.expectEqual(EventKind.press, kindOf(.{ .button = sdl_button_left, .down = true, .clicks = 3 }).?);
    try std.testing.expectEqual(EventKind.release, kindOf(.{ .button = sdl_button_left, .down = false, .clicks = 3 }).?);
    // The right button has no double click of its own.
    try std.testing.expectEqual(EventKind.right_press, kindOf(.{ .button = sdl_button_right, .down = true, .clicks = 2 }).?);
}

test "kindOf: a tool with no use for a double click gets the second click of a fast pair as a press and a release (WR-C03)" {
    try std.testing.expectEqual(EventKind.press, kindOf(.{ .button = sdl_button_left, .down = true, .clicks = 2, .wants_double_click = false }).?);
    try std.testing.expectEqual(EventKind.release, kindOf(.{ .button = sdl_button_left, .down = false, .clicks = 2, .wants_double_click = false }).?);
}

test "staleGesture: a right gesture ends once the right button is no longer down, independently of left and middle" {
    try std.testing.expect(staleGesture(0, false, false, true).end_right);
    try std.testing.expect(!staleGesture(sdl_button_rmask, false, false, true).end_right);
    try std.testing.expect(!staleGesture(0, false, false, false).end_right);
    // Left and middle held do not keep a right gesture alive, and a right
    // button held does not keep a left one.
    try std.testing.expect(staleGesture(sdl_button_lmask | sdl_button_mmask, true, true, true).end_right);
    const right_only = staleGesture(sdl_button_rmask, true, true, true);
    try std.testing.expect(right_only.end_pan);
    try std.testing.expect(right_only.end_left);
    try std.testing.expect(!right_only.end_right);
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
pub const InputEventKind = enum { mouse_button, mouse_motion, mouse_wheel, key, pinch, other };

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
        // A pinch is a two-finger trackpad gesture on the map, not a mouse
        // gesture the view opened - the same "over a panel is the panel's"
        // rule as a wheel, and never overridden by gesture_active.
        .pinch => !capture.mouse,
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
    try std.testing.expect(!shouldDeliver(.pinch, busy, false));
    try std.testing.expect(!shouldDeliver(.pinch, busy, true));
    try std.testing.expect(shouldDeliver(.pinch, free, false));
}

/// One line of Help > Keys and tools: the keys and what they do.
pub const KeyHelp = struct { keys: [:0]const u8, what: [:0]const u8 };

/// The view's own key and pointer map, for Help > Keys and tools (PARITY H1).
/// It mirrors `View.handleKey` and the mouse wiring in view.zig, and lives
/// here, with the pure parts, so the window and its test need no SDL. The
/// tools' own digit keys are not here: the help window lists them from
/// tool_registry.zig's entries, so they cannot drift.
pub const key_help = [_]KeyHelp{
    .{ .keys = "W A S D, arrow keys", .what = "Pan the camera" },
    .{ .keys = "Mouse wheel, two-finger swipe", .what = "Pan the camera (speed in Edit > Settings)" },
    .{ .keys = "Shift + wheel, pinch", .what = "Zoom at the pointer" },
    .{ .keys = "Home", .what = "Reset the view (zoom and rotation)" },
    .{ .keys = "Left button", .what = "Use the active tool; Select picks, drags and rubber-bands" },
    .{ .keys = "Middle button, Alt + left button", .what = "Heights: level; Damage: repair to full" },
    .{ .keys = "Right button", .what = "Select: deselect; Heights: lower; Damage: heal" },
    .{ .keys = "Ctrl + click", .what = "Select: add to the selection; Roads, Entrenchment: right click on a trackpad" },
    .{ .keys = "Q / E", .what = "Turn the selection or the placer left / right" },
    .{ .keys = "Delete, Backspace", .what = "Delete the selection" },
    .{ .keys = "Enter, Space, double click", .what = "Open Properties on the selection; finish or toggle in the Vector tools" },
    .{ .keys = "Insert", .what = "Insert a point or a player" },
    .{ .keys = "Escape", .what = "Cancel the gesture in progress" },
    .{ .keys = "Ctrl/Cmd + Z, Ctrl/Cmd + Y", .what = "Undo, redo (Shift + Z redoes too)" },
    .{ .keys = "Ctrl/Cmd + N, W", .what = "New map, close the map" },
    .{ .keys = "Ctrl/Cmd + Shift + X, B", .what = "Save as XML, save as BZM" },
    .{ .keys = "Ctrl/Cmd + U", .what = "Update Map" },
    .{ .keys = "F5", .what = "Test in game" },
};

test "the help's key map names every key the view handles, with no empty line" {
    try std.testing.expect(key_help.len >= 15);
    for (key_help) |line| {
        try std.testing.expect(line.keys.len != 0);
        try std.testing.expect(line.what.len != 0);
    }
    // The keys handleKey takes by name appear in the list.
    inline for (.{ "Home", "Delete", "Q / E", "Insert", "Escape", "Enter", "Space", "F5" }) |key| {
        var found = false;
        for (key_help) |line| {
            if (std.mem.indexOf(u8, line.keys, key) != null) found = true;
        }
        try std.testing.expect(found);
    }
}
