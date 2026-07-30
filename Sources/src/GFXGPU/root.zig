const std = @import("std");
const sdl3 = @import("sdl3");

pub const Renderer = @import("renderer.zig").Renderer;
pub const shader_manifest = @import("shader_manifest.zig");
pub const shaders = @import("shaders.zig");
pub const sdl_bindings = sdl3;
pub const abi = @import("abi.zig");
pub const error_codes = @import("error.zig");
pub const handles = @import("handles.zig");
pub const formats = @import("formats.zig");
pub const vertex_layout = @import("vertex_layout.zig");
pub const render_state = @import("render_state.zig");
pub const pipeline_key = @import("pipeline_key.zig");
pub const math_convert = @import("math_convert.zig");
pub const sdl = @import("sdl.zig");
pub const device = @import("device.zig");
pub const surface = @import("surface.zig");
pub const frame = @import("frame.zig");
pub const transfer = @import("transfer.zig");
pub const lifetime = @import("lifetime.zig");
pub const buffers = @import("buffers.zig");
pub const textures = @import("textures.zig");
pub const samplers = @import("samplers.zig");
pub const targets = @import("targets.zig");
pub const readback = @import("readback.zig");

pub export fn gfxgpu_get_api(requested_version: u32, out_api: ?*abi.Api) callconv(.c) abi.Result {
    return abi.gfxgpu_get_api(requested_version, out_api);
}

test "renderer context initializes and deinitializes without SDL startup" {
    var renderer = Renderer.init(std.testing.allocator);
    renderer.deinit();
}
