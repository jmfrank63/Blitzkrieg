const std = @import("std");

pub const Family = enum {
    ui,
    unlit,
    alpha_test,
    alpha_blend,
    particle,
    lightmap,
    lit,
    stencil,
    shadow,
    water,
    special,
};

pub const ShaderEffect = enum { textured, untextured, ui, unlit, unlit_textured, alpha_test };

pub const EffectSpec = struct {
    id: u32,
    family: Family,
    shader_effect: ShaderEffect,
    required_vertex_mask: u32,
    sampler_count: u8,
    uniform_groups: u8,
    fixed_state_overrides: u32,
    caller_state_mask: u32,
};

const state_none = 0;
const state_alpha_test = 1 << 0;
const state_alpha_blend = 1 << 1;
const state_depth_write = 1 << 2;
const state_stencil = 1 << 3;
const state_texture_transform = 1 << 4;

fn make(id: u32, family: Family, shader_effect: ShaderEffect, samplers: u8, state: u32) EffectSpec {
    return .{
        .id = id,
        .family = family,
        .shader_effect = shader_effect,
        .required_vertex_mask = 0,
        .sampler_count = samplers,
        .uniform_groups = 2,
        .fixed_state_overrides = state,
        .caller_state_mask = 0,
    };
}

pub const specs = [_]EffectSpec{
    make(1, .alpha_test, .alpha_test, 1, state_alpha_test | state_alpha_blend),
    make(2, .unlit, .unlit_textured, 1, state_none),
    make(3, .ui, .ui, 1, state_alpha_test | state_alpha_blend),
    make(4, .lightmap, .textured, 2, state_alpha_blend),
    make(5, .lightmap, .textured, 2, state_alpha_blend),
    make(6, .stencil, .untextured, 0, state_stencil),
    make(7, .stencil, .untextured, 0, state_none),
    make(8, .alpha_test, .alpha_test, 1, state_alpha_test | state_alpha_blend),
    make(9, .alpha_blend, .textured, 1, state_alpha_blend),
    make(10, .particle, .textured, 1, state_alpha_test | state_alpha_blend | state_depth_write),
    make(11, .particle, .untextured, 0, state_depth_write),
    make(12, .particle, .textured, 1, state_alpha_test | state_alpha_blend | state_depth_write),
    make(13, .alpha_test, .alpha_test, 1, state_alpha_test | state_alpha_blend),
    make(14, .alpha_blend, .textured, 1, state_alpha_blend | state_depth_write),
    make(15, .alpha_blend, .untextured, 0, state_alpha_blend | state_depth_write),
    make(16, .particle, .textured, 1, state_alpha_blend),
    make(17, .special, .textured, 1, state_none),
    make(18, .special, .textured, 1, state_none),
    make(19, .special, .textured, 1, state_none),
    make(20, .special, .textured, 1, state_none),
    make(21, .ui, .textured, 1, state_alpha_blend),
    make(22, .ui, .textured, 1, state_alpha_blend),
    make(23, .ui, .textured, 1, state_alpha_blend),
    make(100, .water, .textured, 2, state_alpha_blend),
    make(101, .water, .textured, 2, state_none),
    make(102, .water, .textured, 2, state_none),
    make(103, .water, .textured, 1, state_alpha_blend),
    make(104, .water, .textured, 2, state_alpha_test | state_alpha_blend),
    make(110, .shadow, .untextured, 0, state_stencil),
    make(111, .shadow, .textured, 1, state_alpha_test | state_alpha_blend),
    make(112, .shadow, .textured, 1, state_alpha_blend),
    make(113, .shadow, .untextured, 0, state_stencil),
    make(200, .alpha_blend, .textured, 1, state_alpha_test | state_alpha_blend),
    make(300, .stencil, .untextured, 0, state_stencil),
    make(301, .stencil, .untextured, 0, state_stencil),
    make(302, .stencil, .untextured, 0, state_none),
    make(303, .special, .textured, 1, state_alpha_test | state_alpha_blend | state_texture_transform),
    make(304, .special, .textured, 1, state_texture_transform),
    make(310, .special, .untextured, 0, state_none),
    make(311, .special, .untextured, 0, state_none),
    make(312, .special, .untextured, 0, state_none),
    make(313, .special, .untextured, 0, state_none),
    make(314, .special, .untextured, 0, state_none),
    make(315, .special, .untextured, 0, state_none),
    make(316, .special, .untextured, 0, state_none),
    make(317, .special, .untextured, 0, state_none),
    make(318, .special, .untextured, 0, state_none),
    make(319, .special, .untextured, 0, state_none),
    make(320, .special, .untextured, 0, state_none),
    make(321, .special, .untextured, 0, state_none),
    make(322, .special, .untextured, 0, state_none),
    make(323, .special, .untextured, 0, state_none),
    make(324, .special, .untextured, 0, state_none),
    make(325, .special, .untextured, 0, state_none),
    make(326, .special, .untextured, 0, state_none),
    make(327, .special, .untextured, 0, state_none),
    make(328, .special, .untextured, 0, state_none),
    make(329, .special, .untextured, 0, state_none),
};

pub fn find(id: u32) ?EffectSpec {
    for (specs) |spec| if (spec.id == id) return spec;
    return null;
}

pub fn uiHalfPixel(clip_position: [4]f32, viewport_size: [2]f32) [4]f32 {
    return .{ clip_position[0] - clip_position[3] / viewport_size[0], clip_position[1] + clip_position[3] / viewport_size[1], clip_position[2], clip_position[3] };
}

pub fn modulateColor(texture: [4]f32, vertex: [4]f32, draw: [4]f32) [4]f32 {
    return .{ texture[0] * vertex[0] * draw[0], texture[1] * vertex[1] * draw[1], texture[2] * vertex[2] * draw[2], texture[3] * vertex[3] * draw[3] };
}

pub fn alphaPassGreaterEqual(alpha: f32, reference: u8) bool {
    return alpha >= @as(f32, @floatFromInt(reference)) / 255.0;
}

test "effect catalog is complete and unique" {
    for (specs, 0..) |spec, index| {
        switch (spec.shader_effect) {
            .textured, .untextured, .ui, .unlit, .unlit_textured, .alpha_test => {},
        }
        try std.testing.expect(spec.uniform_groups != 0);
        try std.testing.expect(find(spec.id) != null);
        for (specs[index + 1 ..]) |later| try std.testing.expect(spec.id != later.id);
    }
    for ([_]u32{ 310, 311, 312, 313, 314, 315, 316, 317, 318, 319, 320, 321, 322, 323, 324, 325, 326, 327, 328, 329 }) |id|
        try std.testing.expect(find(id) != null);
}

test "UI and alpha reference fixtures" {
    const corrected = uiHalfPixel(.{ 0.0, 0.0, 0.5, 1.0 }, .{ 800.0, 600.0 });
    try std.testing.expectApproxEqAbs(-1.0 / 800.0, corrected[0], 0.000001);
    try std.testing.expectApproxEqAbs(1.0 / 600.0, corrected[1], 0.000001);

    const color = modulateColor(.{ 0.5, 0.25, 1.0, 0.5 }, .{ 0.2, 0.4, 0.6, 0.8 }, .{ 0.5, 1.0, 0.25, 1.0 });
    try std.testing.expectApproxEqAbs(0.05, color[0], 0.000001);
    try std.testing.expectApproxEqAbs(0.1, color[1], 0.000001);
    try std.testing.expectApproxEqAbs(0.15, color[2], 0.000001);
    try std.testing.expectApproxEqAbs(0.4, color[3], 0.000001);
    try std.testing.expect(alphaPassGreaterEqual(1.0 / 255.0, 1));
    try std.testing.expect(!alphaPassGreaterEqual(0.0, 1));
    try std.testing.expect(alphaPassGreaterEqual(200.0 / 255.0, 200));
}
