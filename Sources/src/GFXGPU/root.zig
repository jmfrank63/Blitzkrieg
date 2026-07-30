const std = @import("std");
const sdl3 = @import("sdl3");

pub const Renderer = @import("renderer.zig").Renderer;
pub const sdl = sdl3;
pub const abi = @import("abi.zig");
pub const error_codes = @import("error.zig");

pub export fn gfxgpu_get_api(requested_version: u32, out_api: ?*abi.Api) callconv(.c) abi.Result {
    return abi.gfxgpu_get_api(requested_version, out_api);
}

test "renderer context initializes and deinitializes without SDL startup" {
    var renderer = Renderer.init(std.testing.allocator);
    renderer.deinit();
}
