const sdl3 = @import("sdl3");
const std = @import("std");

const fps = 60;
const screen_width = 640;
const screen_height = 480;
const init_flags = sdl3.InitFlags{ .video = true };

/// Application state to keep track of.
const AppState = struct {
    fps_capper: sdl3.extras.FramerateCapper(f32),
    window: sdl3.video.Window,
    init: sdl3.Init,
    heap_value: *i32,
};

/// Runs once on application startup.
fn init(
    init_data: sdl3.Init,
) !struct { AppState, sdl3.AppResult } {
    try sdl3.init(init_flags);
    errdefer sdl3.quit(init_flags);

    // Create window.
    // We use `errdefer` as we will only free items from the app with a created app state.
    const window = try sdl3.video.Window.init(std.mem.span(init_data.args[0]), screen_width, screen_height, .{});
    errdefer window.deinit();

    // Finally create the application state.
    return .{
        .{
            .fps_capper = .{ .mode = .{ .limited = fps } },
            .window = window,
            .init = init_data,
            .heap_value = try init_data.gpa.create(i32), // Init data comes with an allocator.
        },
        .run,
    };
}

/// Iterate function that is called once every frame.
fn iterate(
    app_state: *AppState,
) !sdl3.AppResult {

    // Delay to maintain FPS, returned delta time not needed.
    const dt = app_state.fps_capper.delay();
    _ = dt;

    // Update loop here.
    const surface = try app_state.window.getSurface();
    try surface.fillRect(null, surface.mapRgb(128, 30, 255));
    try app_state.window.updateSurface();

    return .run;
}

/// Event loop function for when an event is recieved.
fn event(
    app_state: *AppState,
    curr_event: sdl3.events.Event,
) !sdl3.AppResult {
    _ = app_state;

    return switch (curr_event) {
        .quit => .success,
        .terminating => .success,
        else => .run,
    };
}

/// Called when quitting.
fn quit(
    app_state: ?*AppState,
    result: sdl3.AppResult,
) void {
    _ = result;

    // We only want to de-initialize if initialization was successful.
    if (app_state) |state| {
        state.init.gpa.destroy(state.heap_value);
        state.window.deinit();
    }
    sdl3.quit(init_flags);
    sdl3.shutdown();
}

/// Manual callback control.
pub fn main() u8 {

    // Example on how to use callbacks for a project that does not use them as a build option.
    // For an example on how to create a project using the callbacks without this, see the template.
    sdl3.main_funcs.setMainReady();
    var args = [_:null]?[*:0]u8{
        @constCast("Hello SDL3"),
    };
    return sdl3.main_funcs.enterAppMainCallbacks(&args, AppState, init, iterate, event, quit);
}
