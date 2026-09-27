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

    pub fn clamp(self: *Camera, map: MapSize) void {
        const max_x = @as(f32, @floatFromInt(map.width_tiles)) * world_cell_size;
        const max_y = @as(f32, @floatFromInt(map.height_tiles)) * world_cell_size;
        self.x = std.math.clamp(self.x, 0, max_x);
        self.y = std.math.clamp(self.y, 0, max_y);
    }
};

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
        .mouse_button, .mouse_motion, .mouse_wheel => gesture_active or !capture.mouse,
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
