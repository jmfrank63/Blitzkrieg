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

pub const ShaderEffect = enum { textured, untextured };

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
    make(1, .alpha_test, .textured, 1, state_alpha_test | state_alpha_blend),
    make(2, .lit, .textured, 1, state_none),
    make(3, .ui, .textured, 1, state_alpha_test | state_alpha_blend),
    make(4, .lightmap, .textured, 2, state_alpha_blend),
    make(5, .lightmap, .textured, 2, state_alpha_blend),
    make(6, .stencil, .untextured, 0, state_stencil),
    make(7, .stencil, .untextured, 0, state_none),
    make(8, .alpha_test, .textured, 1, state_alpha_test | state_alpha_blend),
    make(9, .alpha_blend, .textured, 1, state_alpha_blend),
    make(10, .particle, .textured, 1, state_alpha_test | state_alpha_blend | state_depth_write),
    make(11, .particle, .untextured, 0, state_depth_write),
    make(12, .particle, .textured, 1, state_alpha_test | state_alpha_blend | state_depth_write),
    make(13, .alpha_test, .untextured, 0, state_alpha_test | state_alpha_blend),
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

test "effect catalog is complete and unique" {
    for (specs, 0..) |spec, index| {
        try std.testing.expect(spec.shader_effect == .textured or spec.shader_effect == .untextured);
        try std.testing.expect(spec.uniform_groups != 0);
        try std.testing.expect(find(spec.id) != null);
        for (specs[index + 1 ..]) |later| try std.testing.expect(spec.id != later.id);
    }
    for ([_]u32{ 310, 311, 312, 313, 314, 315, 316, 317, 318, 319, 320, 321, 322, 323, 324, 325, 326, 327, 328, 329 }) |id|
        try std.testing.expect(find(id) != null);
}
