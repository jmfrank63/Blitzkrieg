const sdl3 = @import("sdl3");

pub fn main() !void {

    // Opening a link works and you can do it.
    try sdl3.openURL("https://codeberg.org/7Games/zig-sdl3");
}
