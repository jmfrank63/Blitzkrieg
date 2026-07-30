const sdl3 = @import("sdl3");
const std = @import("std");

/// State used for the application.
const State = struct {
    allocator: std.mem.Allocator,
    window: sdl3.video.Window,
    quit: bool = false,
    last_file: ?[:0]const u8 = null,
    last_file_filter: ?usize = null,
    last_folder: ?[:0]const u8 = null,
    is_file: bool = true,
};

// Called whenever a native dialog is shown.
fn fileCallback(
    user_data: ?*State,
    file_list: ?[]const [*:0]const u8,
    filter: ?usize,
    err: bool,
) void {

    // Error encountered, reset.
    if (err) {
        if (user_data) |user| {
            if (user.is_file) {
                user.last_file = "[dialog error]";
                user.last_file_filter = null;
            } else {
                user.last_folder = "[dialog error]";
            }
        }
        showMenu(@alignCast(@ptrCast(user_data))) catch {};
        return;
    }

    // Set new data.
    const data = user_data.?;
    if (data.is_file) {
        data.last_file = std.fmt.allocPrintSentinel(data.allocator, "{s}", .{if (file_list) |val| val[0] else "[null]"}, 0) catch "[allocation error]";
        data.last_file_filter = filter;
    } else {
        data.last_folder = std.fmt.allocPrintSentinel(data.allocator, "{s}", .{if (file_list) |val| val[0] else "[null]"}, 0) catch "[allocation error]";
    }
    showMenu(data) catch {};
}

// Logic for showing the main menu popup.
fn showMenu(
    state: *State,
) !void {
    const selected = try sdl3.message_box.show(.{
        .buttons = &.{
            .{
                .text = "Select File",
                .value = 0,
            },
            .{
                .text = "Select Folder",
                .value = 1,
            },
            .{
                .text = "Save File",
                .value = 2,
            },
            .{
                .text = "Save File 2",
                .value = 3,
            },
            .{
                .text = "Quit",
                .value = -1,
            },
        },
        .color_scheme = null,
        .flags = .{},
        .parent_window = state.window,
        .title = "Dialog Example",
        .message = try std.fmt.allocPrintSentinel(
            state.allocator,
            "Last file: {s}\nLast file filter: {s}\nLast folder: {s}",
            .{
                if (state.last_file) |val| val else "[null]",
                if (state.last_file_filter) |val| std.fmt.allocPrintSentinel(state.allocator, "{d}", .{val}, 0) catch "[allocation error]" else "[null]",
                if (state.last_folder) |val| val else "[null]",
            },
            0,
        ),
    });

    // Match the buttons earlier to actions.
    switch (selected) {
        -1 => state.quit = true,
        0 => {
            state.is_file = true;
            sdl3.dialog.showOpenFile(State, fileCallback, state, state.window, &.{
                .{
                    .name = "PNG Images",
                    .pattern = "png",
                },
                .{
                    .name = "Any File",
                    .pattern = "*",
                },
            }, state.last_file, false);
        },
        1 => {
            state.is_file = false;
            sdl3.dialog.showOpenFolder(State, fileCallback, state, state.window, state.last_folder, false);
        },
        2 => {
            state.is_file = true;
            sdl3.dialog.showSaveFile(State, fileCallback, state, state.window, &.{
                .{
                    .name = "PNG Images",
                    .pattern = "png",
                },
                .{
                    .name = "Any File",
                    .pattern = "*",
                },
            }, state.last_file);
        },
        3 => {
            state.is_file = true;
            const props = try sdl3.dialog.showWithProperties(.save_file, State, fileCallback, state, .{});
            defer props.deinit();
        },
        else => {},
    }
}

pub fn main() !void {
    defer sdl3.shutdown();

    // Memory is used pretty terribly in this app with constant leaks but it's just an example.
    var arena = std.heap.ArenaAllocator.init(std.heap.smp_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    // Initialize SDL.
    const init_flags = sdl3.InitFlags{ .video = true };
    try sdl3.init(init_flags);
    defer sdl3.quit(init_flags);

    // Setup the window.
    const window = try sdl3.video.Window.init("Dialog Example Backing", 500, 300, .{
        .transparent = true,
    });
    defer window.deinit();

    // Setup main state and show menu.
    var state = State{
        .allocator = allocator,
        .window = window,
    };
    try showMenu(&state);

    // Wait until quit.
    while (!state.quit) {
        switch (try sdl3.events.waitAndPop()) {
            .terminating => break,
            .quit => break,
            else => {},
        }
    }
}
