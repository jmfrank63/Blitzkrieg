const sdl3 = @import("sdl3");

fn playGame() !bool {

    // Ask the question.
    const val = try sdl3.message_box.show(.{
        .flags = .{},
        .title = "Question:",
        .message = "What is my favorite color?",
        .parent_window = null,
        .buttons = &.{ .{
            .text = "Red",
            .value = 0,
        }, .{
            .text = "Orange",
            .value = 0,
        }, .{
            .text = "Yellow",
            .value = 0,
        }, .{
            .text = "Green",
            .value = 0,
        }, .{
            .text = "Blue",
            .value = 0,
        }, .{
            .text = "Purple",
            .value = 1,
        } },
        .color_scheme = null,
    });

    // Purple is the only correct answer, show wrong answer and show a custom color scheme.
    if (val != 1) {
        _ = try sdl3.message_box.show(.{
            .flags = .{ .buttons_right_to_left = true },
            .title = "Wrong!",
            .message = "You have chosen the wrong answer. Please try again.",
            .parent_window = null,
            .buttons = &.{
                .{
                    .text = "I Understand",
                    .value = 0,
                },
            },
            .color_scheme = .{
                .background = .{ .r = 10, .g = 10, .b = 10 },
                .text = try sdl3.message_box.ColorScheme.Color.fromHex("ffffff"),
                .button_border = .{ .r = 75, .g = 55, .b = 50 },
                .button_background = .{ .r = 160, .g = 20, .b = 20 },
                .button_selected = try sdl3.message_box.ColorScheme.Color.fromHex("fdfd22"),
            },
        });
        return false;
    }

    // Purple has been answered, show popup with custom color scheme.
    else {
        _ = try sdl3.message_box.show(.{
            .flags = .{ .buttons_left_to_right = true },
            .title = "Correct!",
            .message = "You have guessed correctly. Yippee!",
            .parent_window = null,
            .buttons = &.{
                .{
                    .text = "Bye!",
                    .value = 0,
                },
            },
            .color_scheme = .{
                .background = .{ .r = 195, .g = 195, .b = 195 },
                .text = try sdl3.message_box.ColorScheme.Color.fromHex("660aa8"),
                .button_border = .{ .r = 50, .g = 75, .b = 55 },
                .button_background = .{ .r = 20, .g = 160, .b = 20 },
                .button_selected = try sdl3.message_box.ColorScheme.Color.fromHex("fdfd22"),
            },
        });
        return true;
    }
}

pub fn main() !void {
    try sdl3.message_box.showSimple(.{}, "Start!", "Get ready to play the game.", null);

    // Play game until the correct guess is answered.
    while (true) {
        if (try playGame())
            break;
    }
}
