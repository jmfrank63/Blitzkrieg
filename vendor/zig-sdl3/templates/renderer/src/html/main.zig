const sdl3 = @import("sdl3");
const std = @import("std");
const builtin = @import("builtin");

// Use main callbacks.
comptime {
    _ = sdl3.main_callbacks;
}

// https://www.pexels.com/photo/green-trees-on-the-field-1630049/
const my_image = @embedFile("data/trees.jpeg");

const fps = 60;
const window_width = 640;
const window_height = 480;
const init_flags = sdl3.InitFlags{ .video = true };

// Disable main hack.
pub const _start = void;
pub const WinMainCRTStartup = void;

/// For logging system messages.
const log_app = sdl3.log.Category.application;

/// Sample structure to use to hold our app state.
const AppState = struct {
    frame_capper: sdl3.extras.FramerateCapper(f32),
    window: sdl3.video.Window,
    renderer: sdl3.render.Renderer,
    tree_tex: sdl3.render.Texture,
};

/// Do our initialization logic here.
///
/// ## Function Parameters
/// * `init_data`: Initialization data provided to the application.
///
/// ## Return Value
/// Returns the app state along if the app should continue running, or result in success or failure.
///
/// ## Remarks
/// Note that for further callbacks (except for `quit()`), we assume that we did end up setting `app_state`.
/// If this function does not return `AppResult.run` or errors, then `quit()` will be invoked.
/// Do not worry about logging errors from SDL yourself and just use `try` and `catch` as you please.
/// If you set the error callback for every thread, then zig-sdl3 will be automatically logging errors.
pub fn init(
    init_data: sdl3.Init,
) !struct { AppState, sdl3.AppResult } {
    _ = init_data;

    try sdl3.init(init_flags);
    errdefer sdl3.quit(init_flags);

    // Setup logging.
    sdl3.log.setAllPriorities(.info);
    sdl3.log.setLogOutputFunction(void, &sdl3.extras.loggers.zigLog, null);

    try log_app.logInfo("Starting application...", .{});

    // Setup initial data.
    const window, const renderer = try sdl3.render.Renderer.initWithWindow(
        "Hello SDL3",
        window_width,
        window_height,
        .{},
    );
    errdefer renderer.deinit();
    errdefer window.deinit();
    var frame_capper = sdl3.extras.FramerateCapper(f32){ .mode = .{ .unlimited = {} } };
    renderer.setVSync(.{ .on_each_num_refresh = 1 }) catch {

        // We don't want to run at unlimited FPS, cap frame rate to the default FPS if vsync is not available so we don't burn CPU time.
        frame_capper.mode = .{ .limited = fps };
    };
    const tree_tex = try sdl3.image.loadTextureIo(
        renderer,
        try sdl3.io_stream.Stream.initFromConstMem(my_image),
        true,
    );
    errdefer tree_tex.deinit();

    // Prove error handling works.
    const dummy: ?sdl3.video.Window = sdl3.video.Window.fromId(99999) catch null;
    _ = dummy;

    // Done setting up.
    try log_app.logInfo("Finished initializing", .{});
    return .{
        .{
            .frame_capper = frame_capper,
            .window = window,
            .renderer = renderer,
            .tree_tex = tree_tex,
        },
        .run,
    };
}

/// Do our render and update logic here.
///
/// ## Function Parameters
/// * `app_state`: Application state set from `init()`.
///
/// ## Return Value
/// Returns if the app should continue running, or result in success or failure.
///
/// ## Remarks
/// If this function does not return `AppResult.run` or errors, then `quit()` will be invoked.
/// We assume that `app_state` was set by `init()`.
/// If this function takes too long, your application will lag.
pub fn iterate(
    app_state: *AppState,
) !sdl3.AppResult {
    const dt = app_state.frame_capper.delay();
    _ = dt; // We don't need dt for this example, but might be useful to you.

    // Draw main scene.
    try app_state.renderer.setDrawColor(.{ .r = 128, .g = 30, .b = 255, .a = 255 });
    try app_state.renderer.clear();
    const border = 10;
    try app_state.renderer.renderTexture(app_state.tree_tex, null, .{
        .x = border,
        .y = border,
        .w = window_width - border * 2,
        .h = window_height - border * 2,
    });
    try app_state.renderer.setDrawColor(.{ .r = 0, .g = 0, .b = 0, .a = 255 });

    // Draw debug FPS.
    var fps_text_buf: [32]u8 = undefined;
    const fps_text = std.fmt.bufPrintZ(&fps_text_buf, "FPS: {d}", .{app_state.frame_capper.getObservedFps()}) catch "[Err]";
    try app_state.renderer.renderDebugText(.{ .x = 0, .y = 0 }, fps_text);

    // Finish and return.
    try app_state.renderer.present();
    return .run;
}

/// Handle events here.
///
/// ## Function Parameter
/// * `app_state`: Application state set from `init()`.
/// * `event`: Event that the application has just received.
///
/// ## Return Value
/// Returns if the app should continue running, or result in success or failure.
///
/// ## Remarks
/// If this function does not return `AppResult.run` or errors, then `quit()` will be invoked.
/// We assume that `app_state` was set by `init()`.
/// If this function takes too long, your application will lag.
pub fn event(
    app_state: *AppState,
    curr_event: sdl3.events.Event,
) !sdl3.AppResult {
    _ = app_state;
    switch (curr_event) {
        .terminating => return .success,
        .quit => return .success,
        else => {},
    }
    return .run;
}

/// Quit logic here.
///
/// ## Function Parameters
/// * `app_state`: Application state if it was set by `init()`, or `null` if `init()` if it didn't return an error.
/// * `result`: Result indicating the success of the application. Should never be `AppResult.run`.
///
/// ## Remarks
/// Make sure you clean up any resources here.
/// Or don't the OS would take care of it anyway but any leak detection you use will yell at you :>
pub fn quit(
    app_state: ?*AppState,
    result: sdl3.AppResult,
) void {
    _ = result;
    if (app_state) |val| {
        val.tree_tex.deinit();
        val.renderer.deinit();
        val.window.deinit();
    }

    sdl3.quit(init_flags);
    sdl3.shutdown();
}
