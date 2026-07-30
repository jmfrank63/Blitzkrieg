const std = @import("std");
const sdl3 = @import("sdl3");

pub const Renderer = @import("renderer.zig").Renderer;
pub const sdl = sdl3;
pub const abi = @import("abi.zig");
pub const error_codes = @import("error.zig");

test "renderer context initializes and deinitializes without SDL startup" {
    var renderer = Renderer.init(std.testing.allocator);
    renderer.deinit();
}
