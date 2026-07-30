const sdl3 = @import("sdl3");
const std = @import("std");

const fps = 60;
const screen_width = 640;
const screen_height = 480;

fn sdlErrSdlMessageBox(
    err: ?[]const u8,
) void {
    var buf: [4096]u8 = undefined;
    const err_msg = if (err) |val| std.fmt.bufPrintZ(&buf, "{s}", .{val}) catch "SDL message too long" else "Unknown SDL error";
    sdl3.message_box.showSimple(.{ .error_dialog = true }, "SDL Error", err_msg, null) catch {};
}

fn sdlLogSdlMessageBox(
    user_data: ?*void,
    category: sdl3.log.Category,
    priority: ?sdl3.log.Priority,
    message: [:0]const u8,
) void {
    _ = user_data;
    const category_str: ?[]const u8 = switch (category) {
        .application => "Application",
        .errors => "Errors",
        .assert => "Assert",
        .system => "System",
        .audio => "Audio",
        .video => "Video",
        .render => "Render",
        .input => "Input",
        .testing => "Testing",
        .gpu => "Gpu",
        else => null,
    };
    const priority_str: [:0]const u8 = if (priority) |val| switch (val) {
        .trace => "Trace",
        .verbose => "Verbose",
        .debug => "Debug",
        .info => "Info",
        .warn => "Warn",
        .err => "Error",
        .critical => "Critical",
    } else "Unknown";
    var buf: [4096]u8 = undefined;
    const too_long = "SDL message too long";
    const text = if (category_str) |val| std.fmt.bufPrintZ(&buf, "{s}: {s}", .{ val, message }) catch too_long else message;
    sdl3.message_box.showSimple(
        .{
            .information_dialog = priority == .info,
            .warning_dialog = priority == .warn,
            .error_dialog = priority == .err,
        },
        priority_str,
        text,
        null,
    ) catch {};
}

pub fn main() !void {
    defer sdl3.shutdown();

    // Setup logging.
    // Take a look at `sdl3.extras` for plenty of useful log functions, both debug prints and zig logs are supported!
    sdl3.log.setAllPriorities(.warn);
    sdl3.errors.error_callback = &sdlErrSdlMessageBox; // Note that this is thread local and must be set for each thread! Set to null to disable error messages as it defaults to SDL logging.
    sdl3.log.setLogOutputFunction(void, &sdlLogSdlMessageBox, null);

    // Initialize SDL with subsystems you need here.
    const init_flags = sdl3.InitFlags{ .video = true };
    try sdl3.init(init_flags);
    defer sdl3.quit(init_flags);

    // Cause some messages.
    try sdl3.log.Category.application.logWarn("Hello {s}", .{"World"});
    sdl3.log.Category.application.setPriority(.info);
    try sdl3.log.Category.application.logInfo("Normal message", .{});
    _ = sdl3.video.Window.fromId(99) catch {}; // Deliberately cause an error.

    // Initial window setup.
    const window = try sdl3.video.Window.init("Hello SDL3", screen_width, screen_height, .{});
    defer window.deinit();

    // Useful for limiting the FPS and getting the delta time.
    var fps_capper = sdl3.extras.FramerateCapper(f32){ .mode = .{ .limited = fps } };

    var quit = false;
    while (!quit) {

        // Delay to limit the FPS, returned delta time not needed.
        const dt = fps_capper.delay();
        _ = dt;

        // Update logic.
        const surface = try window.getSurface();
        try surface.fillRect(null, surface.mapRgb(128, 30, 255));
        try window.updateSurface();

        // Event logic.
        while (sdl3.events.poll()) |event|
            switch (event) {
                .quit => quit = true,
                .terminating => quit = true,
                else => {},
            };
    }
}
