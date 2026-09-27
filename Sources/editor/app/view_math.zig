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
    pub fn scroll(self: *Camera, dir: Scroll, dt_seconds: f32, map: MapSize) void {
        const delta = scroll_speed * dt_seconds;
        if (dir.left) self.x -= delta;
        if (dir.right) self.x += delta;
        if (dir.up) self.y -= delta;
        if (dir.down) self.y += delta;
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
    try std.testing.expect(camera.x < 100);
    camera = .{ .x = 0, .y = 0 };
    camera.scroll(.{ .left = true, .up = true }, 10, .{ .width_tiles = 96, .height_tiles = 96 });
    try std.testing.expectEqual(@as(f32, 0), camera.x);
    try std.testing.expectEqual(@as(f32, 0), camera.y);
}

const sdl_left_down: ButtonEvent = .{ .button = sdl_button_left, .down = true };
const sdl_right_down: ButtonEvent = .{ .button = sdl_button_right, .down = true };

test "a mouse button maps to a tool event, and only the left button edits" {
    try std.testing.expectEqual(EventKind.press, kindOf(sdl_left_down).?);
    try std.testing.expect(kindOf(sdl_right_down) == null);
}

test "scroll clamps at the far edge too" {
    var camera: Camera = .{ .x = 3000, .y = 3000 };
    camera.scroll(.{ .right = true, .down = true }, 10, .{ .width_tiles = 96, .height_tiles = 96 });
    const max = @as(f32, 96) * world_cell_size;
    try std.testing.expectEqual(max, camera.x);
    try std.testing.expectEqual(max, camera.y);
}

test "a release maps to a tool event too" {
    try std.testing.expectEqual(EventKind.release, kindOf(.{ .button = sdl_button_left, .down = false }).?);
}
