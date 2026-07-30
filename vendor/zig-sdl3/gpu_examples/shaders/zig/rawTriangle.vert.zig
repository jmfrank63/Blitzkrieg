const std = @import("std");

const color_out = @extern(
    *addrspace(.output) @Vector(4, f32),
    .{
        .name = "color",
        .decoration = .{
            .location = 0,
        },
    },
);

export fn main() callconv(.spirv_vertex) void {
    switch (std.gpu.vertex_index) {
        0 => {
            std.gpu.position_out.* = .{ -1, -1, 0, 1 };
            color_out.* = .{ 1, 0, 0, 1 };
        },
        1 => {
            std.gpu.position_out.* = .{ 1, -1, 0, 1 };
            color_out.* = .{ 0, 1, 0, 1 };
        },
        else => {
            std.gpu.position_out.* = .{ 0, 1, 0, 1 };
            color_out.* = .{ 0, 0, 1, 1 };
        },
    }
}
