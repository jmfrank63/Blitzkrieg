const gputils = @import("gputils.zig");
const std = @import("std");

const texture = gputils.Sampler2d(2, 0);

const tex_coord_in = @extern(
    *addrspace(.input) @Vector(2, f32),
    .{
        .name = "tex_coord",
        .decoration = .{
            .location = 0,
        },
    },
);

const color_out = @extern(
    *addrspace(.output) @Vector(4, f32),
    .{
        .name = "color",
        .decoration = .{
            .location = 0,
        },
    },
);

export fn main() callconv(.spirv_fragment) void {
    color_out.* = texture.texture(tex_coord_in.*);
}
