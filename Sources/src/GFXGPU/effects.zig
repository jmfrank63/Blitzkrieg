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

pub const ShaderEffect = enum { textured, untextured, ui, unlit, unlit_textured, alpha_test, transparent_multiply, transparent_alpha, transparent_additive, particle_additive, particle_modulate, lightmap_modulate, lightmap_complement, lighting, stencil_write, stencil_test, shadow_sprite, shadow_mesh, water, water_single, water_alpha, special_video, special_transform, special_depth };
pub const BlendMode = enum { replace, multiply, straight_alpha, additive };
pub const FogMode = enum { none, linear };

// How an effect combines its two texture stages, read off the D3DTSS_* states
// CGraphicsEngine::SetShadingEffect programs:
//   101  stage0 = tileset MODULATE diffuse, stage1 = noise MODULATE current,
//        alpha from stage0        -> the noise shades the ground
//   100  stage0 = tileset MODULATE diffuse, stage1 colour = CURRENT and
//        alpha = crosset texture  -> the crosset masks a tile transition
//   102  identical stage setup to 100, alpha blended
//   104  stage0 = noise SELECTARG1 (no diffuse), stage1 alpha = crosset,
//        plus ALPHAREF 50 GREATEREQUAL
pub const Combine = enum(u32) { single = 0, modulate_second = 1, mask_alpha = 2, mask_alpha_test = 3 };

// Whether an effect turns the stage-0 texture matrix on. CGraphicsEngine sets
// D3DTTFF_COUNT2 for 303, the flowing river layers, and D3DTTFF_DISABLE for 304,
// which exists only to turn it back off again.
pub fn usesTextureTransform(id: u32) bool {
    const spec = find(id) orelse return false;
    return (spec.fixed_state_overrides & state_texture_transform) != 0;
}

// Stage-0 D3DTSS_COLOROP. Nearly every effect MODULATEs the texture with the
// diffuse colour, but CGraphicsEngine::SetShadingEffect gives 20 -- the vehicle
// tracks -- D3DTOP_ADD against SRCBLEND=DESTCOLOR/DESTBLEND=ZERO. That pairing
// is how a track fades: the multiply darkens the ground by the texture, and as
// the trace ages its vertex colour brightens toward white until the sum
// saturates and the multiply leaves the ground untouched. Drawn as MODULATE and
// REPLACE instead, the tracks appeared as the raw black and white texture and
// never faded.
// D3DRS_ALPHAREF for the effects that enable D3DRS_ALPHATESTENABLE, every one of
// them with D3DCMP_GREATEREQUAL. There is no fixed-function alpha test here, so
// the fragment shader discards below this value. Without the discard the fully
// transparent corners of a tree or grass sprite were still shaded and still
// wrote depth, so the sprite's quad cut a square out of whatever stood behind
// it. 104 is left to textured_dual.hlsl, which tests the crosset's alpha rather
// than the final one.
pub fn alphaRefFor(id: u32) u8 {
    return switch (id) {
        1 => 200,
        13 => 10,
        12 => 5,
        111, 200 => 50,
        3, 8, 10, 19, 21, 303 => 1,
        else => 0,
    };
}

// CGraphicsEngine::SetShadingEffect applies each effect as a delta over the
// current D3D render state, so 110 opens the shadow pass, 111 and 112 draw
// inside it, and 113 closes it again. Only the effects that actually write
// D3DRS_STENCILENABLE report a change here; the rest inherit.
//
// The pass exists because a shadow must darken each pixel exactly once.
// STENCILFUNC EQUAL against a reference of 0 with STENCILPASS INCRSAT lets the
// first fragment through and stamps the pixel, so every later fragment covering
// it fails the test. Without it, overlapping silhouette polygons -- a hull, its
// turret and its gun -- compound at MESH_SHADOW_DENSITY 0.5 into 0.75, then
// 0.875, and the vehicle wears a dark blot.
pub const StencilMode = enum(u32) { off = 0, darken_once = 1 };

pub fn stencilChangeFor(id: u32) ?StencilMode {
    return switch (id) {
        6, 110 => .darken_once,
        7, 113 => .off,
        else => null,
    };
}

// D3DRS_ZENABLE, likewise a delta. 110 drops the depth test for the shadow pass
// and 113 restores it; 200 draws its additive sprites without one.
pub fn depthTestChangeFor(id: u32) ?bool {
    return switch (id) {
        110, 113 => id == 113,
        200 => false,
        else => null,
    };
}

pub const ColorOp = enum(u32) { modulate = 0, add = 1 };

pub fn colorOpFor(id: u32) ColorOp {
    return switch (id) {
        20 => .add,
        else => .modulate,
    };
}

pub fn combineFor(id: u32) Combine {
    return switch (id) {
        101 => .modulate_second,
        100, 102 => .mask_alpha,
        104 => .mask_alpha_test,
        else => .single,
    };
}

pub const EffectSpec = struct {
    id: u32,
    family: Family,
    shader_effect: ShaderEffect,
    required_vertex_mask: u32,
    sampler_count: u8,
    uniform_groups: u8,
    fixed_state_overrides: u32,
    caller_state_mask: u32,
    blend: BlendMode,
    depth_write: bool,
    fog: FogMode,
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
        .blend = if ((state & state_alpha_blend) != 0) .straight_alpha else .replace,
        .depth_write = true,
        .fog = .none,
    };
}

fn withPipelinePolicy(base: EffectSpec, blend_mode: BlendMode, depth_write: bool, fog: FogMode) EffectSpec {
    var result = base;
    result.blend = blend_mode;
    result.depth_write = depth_write;
    result.fog = fog;
    return result;
}

pub const specs = [_]EffectSpec{
    make(1, .alpha_test, .alpha_test, 1, state_alpha_test | state_alpha_blend),
    make(2, .lit, .lighting, 1, state_none),
    make(3, .ui, .ui, 1, state_alpha_test | state_alpha_blend),
    make(4, .lightmap, .lightmap_complement, 2, state_alpha_blend),
    make(5, .lightmap, .lightmap_modulate, 2, state_alpha_blend),
    make(6, .stencil, .stencil_write, 0, state_stencil),
    make(7, .stencil, .stencil_test, 0, state_none),
    make(8, .alpha_test, .alpha_test, 1, state_alpha_test | state_alpha_blend),
    withPipelinePolicy(make(9, .alpha_blend, .transparent_multiply, 1, state_alpha_blend), .multiply, false, .none),
    withPipelinePolicy(make(10, .particle, .particle_additive, 1, state_alpha_test | state_alpha_blend | state_depth_write), .additive, false, .linear),
    make(11, .particle, .untextured, 0, state_depth_write),
    withPipelinePolicy(make(12, .particle, .particle_modulate, 1, state_alpha_test | state_alpha_blend | state_depth_write), .straight_alpha, false, .linear),
    make(13, .alpha_test, .alpha_test, 1, state_alpha_test | state_alpha_blend),
    withPipelinePolicy(make(14, .alpha_blend, .transparent_alpha, 1, state_alpha_blend | state_depth_write), .straight_alpha, false, .linear),
    withPipelinePolicy(make(15, .alpha_blend, .transparent_alpha, 1, state_alpha_blend | state_depth_write), .straight_alpha, false, .linear),
    withPipelinePolicy(make(16, .particle, .particle_additive, 1, state_alpha_blend), .additive, false, .linear),
    make(17, .special, .special_video, 1, state_none),
    make(18, .special, .special_video, 1, state_none),
    make(19, .special, .special_transform, 1, state_none),
    // Vehicle tracks: SRCBLEND=DESTCOLOR, DESTBLEND=ZERO. See colorOpFor.
    withPipelinePolicy(make(20, .special, .special_transform, 1, state_alpha_blend), .multiply, false, .none),
    make(21, .ui, .textured, 1, state_alpha_blend),
    make(22, .ui, .textured, 1, state_alpha_blend),
    make(23, .ui, .textured, 1, state_alpha_blend),
    // 100-104 are the terrain passes, and their blend modes come straight from
    // the D3D states CGraphicsEngine::SetShadingEffect sets. 102 is alpha
    // blended there despite the name, and 103/104 use SRCBLEND=DESTCOLOR with
    // DESTBLEND=ZERO -- a pure multiply, the noise modulating the ground under
    // it. Treating those two as straight alpha painted the noise texture over
    // the terrain as flat grey.
    make(100, .water, .water, 2, state_alpha_blend),
    make(101, .water, .water, 2, state_none),
    withPipelinePolicy(make(102, .water, .water, 2, state_alpha_blend), .straight_alpha, true, .none),
    withPipelinePolicy(make(103, .water, .water_single, 1, state_alpha_blend), .multiply, true, .none),
    withPipelinePolicy(make(104, .water, .water_alpha, 2, state_alpha_test | state_alpha_blend), .multiply, true, .none),
    make(110, .shadow, .stencil_test, 0, state_stencil),
    make(111, .shadow, .shadow_sprite, 1, state_alpha_test | state_alpha_blend),
    make(112, .shadow, .shadow_mesh, 1, state_alpha_blend),
    make(113, .shadow, .stencil_test, 0, state_stencil),
    withPipelinePolicy(make(200, .alpha_blend, .transparent_additive, 1, state_alpha_test | state_alpha_blend), .additive, false, .linear),
    make(300, .stencil, .stencil_write, 0, state_stencil),
    make(301, .stencil, .stencil_test, 0, state_stencil),
    make(302, .stencil, .stencil_test, 0, state_none),
    make(303, .special, .special_transform, 1, state_alpha_test | state_alpha_blend | state_texture_transform),
    // 304 is D3DTTFF_DISABLE: the effect that clears the transform, not one that
    // uses it. Carrying the flag here left the water's scroll matrix applied to
    // everything drawn after the river.
    make(304, .special, .special_transform, 1, state_none),
    make(310, .special, .special_depth, 0, state_none),
    make(311, .special, .special_depth, 0, state_none),
    make(312, .special, .special_depth, 0, state_none),
    make(313, .special, .special_depth, 0, state_none),
    make(314, .special, .special_depth, 0, state_none),
    make(315, .special, .special_depth, 0, state_none),
    make(316, .special, .special_depth, 0, state_none),
    make(317, .special, .special_depth, 0, state_none),
    make(318, .special, .special_depth, 0, state_none),
    make(319, .special, .special_depth, 0, state_none),
    make(320, .special, .special_depth, 0, state_none),
    make(321, .special, .special_depth, 0, state_none),
    make(322, .special, .special_depth, 0, state_none),
    make(323, .special, .special_depth, 0, state_none),
    make(324, .special, .special_depth, 0, state_none),
    make(325, .special, .special_depth, 0, state_none),
    make(326, .special, .special_depth, 0, state_none),
    make(327, .special, .special_depth, 0, state_none),
    make(328, .special, .special_depth, 0, state_none),
    make(329, .special, .special_depth, 0, state_none),
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

pub fn blend(mode: BlendMode, source: [4]f32, destination: [4]f32) [4]f32 {
    return switch (mode) {
        .replace => source,
        .multiply => .{ source[0] * destination[0], source[1] * destination[1], source[2] * destination[2], source[3] * destination[3] },
        .straight_alpha => .{ source[0] * source[3] + destination[0] * (1.0 - source[3]), source[1] * source[3] + destination[1] * (1.0 - source[3]), source[2] * source[3] + destination[2] * (1.0 - source[3]), source[3] + destination[3] * (1.0 - source[3]) },
        .additive => .{ source[0] * source[3] + destination[0], source[1] * source[3] + destination[1], source[2] * source[3] + destination[2], source[3] + destination[3] },
    };
}

pub fn linearFogFactor(depth: f32, start: f32, end: f32) f32 {
    if (end <= start) return 1.0;
    return std.math.clamp((end - depth) / (end - start), 0.0, 1.0);
}

pub fn lightmapCombine(base: [3]f32, lightmap: [3]f32, complement: bool) [3]f32 {
    const factor = if (complement) [3]f32{ 1.0 - lightmap[0], 1.0 - lightmap[1], 1.0 - lightmap[2] } else lightmap;
    return .{ base[0] * factor[0], base[1] * factor[1], base[2] * factor[2] };
}

pub fn lambert(normal: [3]f32, light_direction: [3]f32) f32 {
    const length = @sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (length <= 0.000001) return 0.0;
    const dot = (normal[0] * light_direction[0] + normal[1] * light_direction[1] + normal[2] * light_direction[2]) / length;
    return @max(dot, 0.0);
}

pub fn pointAttenuation(distance: f32, range: f32, a0: f32, a1: f32, a2: f32) f32 {
    if (distance < 0.0 or distance > range) return 0.0;
    return 1.0 / @max(a0 + distance * a1 + distance * distance * a2, 0.000001);
}

pub fn animateWaterUv(uv: [2]f32, velocity: [2]f32, time: f32) [2]f32 {
    return .{ uv[0] + velocity[0] * time, uv[1] + velocity[1] * time };
}

pub fn stencilEqual(value: u8, reference: u8) bool {
    return value == reference;
}

test "effect catalog is complete and unique" {
    for (specs, 0..) |spec, index| {
        switch (spec.shader_effect) {
            .textured, .untextured, .ui, .unlit, .unlit_textured, .alpha_test, .transparent_multiply, .transparent_alpha, .transparent_additive, .particle_additive, .particle_modulate, .lightmap_modulate, .lightmap_complement, .lighting, .stencil_write, .stencil_test, .shadow_sprite, .shadow_mesh, .water, .water_single, .water_alpha, .special_video, .special_transform, .special_depth => {},
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

    // The D3DRS_ALPHAREF each effect sets, straight from
    // CGraphicsEngine::SetShadingEffect. 1 is the sprite cutout the trees and
    // grass use; an effect that reports 0 leaves D3DRS_ALPHATESTENABLE off.
    try std.testing.expectEqual(@as(u8, 200), alphaRefFor(1));
    try std.testing.expectEqual(@as(u8, 10), alphaRefFor(13));
    try std.testing.expectEqual(@as(u8, 5), alphaRefFor(12));
    try std.testing.expectEqual(@as(u8, 50), alphaRefFor(111));
    try std.testing.expectEqual(@as(u8, 50), alphaRefFor(200));
    for ([_]u32{ 3, 8, 10, 19, 21, 303 }) |id| try std.testing.expectEqual(@as(u8, 1), alphaRefFor(id));
    for ([_]u32{ 2, 9, 11, 14, 20, 100, 102 }) |id| try std.testing.expectEqual(@as(u8, 0), alphaRefFor(id));
    // 104 tests the crosset's alpha, not the final one, so textured_dual.hlsl
    // owns its cutout and the shared path must leave it alone.
    try std.testing.expectEqual(@as(u8, 0), alphaRefFor(104));
    try std.testing.expectEqual(Combine.mask_alpha_test, combineFor(104));

    // A sprite whose transparent corner survives the test would write depth and
    // cut a square out of what stands behind it, so the cutout must reject it.
    try std.testing.expect(!alphaPassGreaterEqual(0.0, alphaRefFor(1)));
    try std.testing.expect(!alphaPassGreaterEqual(150.0 / 255.0, alphaRefFor(1)));
    try std.testing.expect(alphaPassGreaterEqual(1.0, alphaRefFor(1)));
}

test "the shadow pass opens and closes as a render-state delta" {
    // 110 opens it, 111 and 112 draw inside it and touch neither state, 113
    // closes it. Anything that reported a change for 111 or 112 would end the
    // pass halfway through and let the shadows compound again.
    try std.testing.expectEqual(StencilMode.darken_once, stencilChangeFor(110).?);
    try std.testing.expectEqual(StencilMode.off, stencilChangeFor(113).?);
    try std.testing.expectEqual(StencilMode.darken_once, stencilChangeFor(6).?);
    try std.testing.expectEqual(StencilMode.off, stencilChangeFor(7).?);
    for ([_]u32{ 111, 112, 1, 3, 20, 100, 300 }) |id| try std.testing.expect(stencilChangeFor(id) == null);

    // D3DRS_ZENABLE travels with it: off for the shadow pass, back on after.
    try std.testing.expectEqual(false, depthTestChangeFor(110).?);
    try std.testing.expectEqual(true, depthTestChangeFor(113).?);
    try std.testing.expectEqual(false, depthTestChangeFor(200).?);
    for ([_]u32{ 111, 112, 6, 7, 1, 20 }) |id| try std.testing.expect(depthTestChangeFor(id) == null);
}

test "vehicle tracks add the diffuse colour and multiply into the ground" {
    try std.testing.expectEqual(ColorOp.add, colorOpFor(20));
    try std.testing.expectEqual(BlendMode.multiply, (find(20) orelse unreachable).blend);
    // Everything else keeps D3DTOP_MODULATE.
    for ([_]u32{ 1, 3, 12, 19, 21, 100, 303 }) |id| try std.testing.expectEqual(ColorOp.modulate, colorOpFor(id));

    // The fade: a track's vertex colour brightens toward white as it ages, and
    // ADD then saturates, so the multiply leaves the ground exactly as it was.
    const ground = [4]f32{ 0.4, 0.5, 0.3, 1.0 };
    const track_texel = [4]f32{ 0.25, 0.25, 0.25, 1.0 };
    const fresh = blend(.multiply, track_texel, ground);
    try std.testing.expectApproxEqAbs(0.1, fresh[0], 0.000001);
    const faded = [4]f32{ 1.0, 1.0, 1.0, 1.0 };
    const gone = blend(.multiply, faded, ground);
    try std.testing.expectApproxEqAbs(ground[0], gone[0], 0.000001);
    try std.testing.expectApproxEqAbs(ground[1], gone[1], 0.000001);
}

test "transparent blend and range fog fixtures" {
    const source = [4]f32{ 0.8, 0.4, 0.2, 0.5 };
    const destination = [4]f32{ 0.2, 0.2, 0.2, 1.0 };
    const alpha = blend(.straight_alpha, source, destination);
    try std.testing.expectApproxEqAbs(0.5, alpha[0], 0.000001);
    try std.testing.expectApproxEqAbs(0.3, alpha[1], 0.000001);
    const additive = blend(.additive, source, destination);
    try std.testing.expectApproxEqAbs(0.6, additive[0], 0.000001);
    const multiply = blend(.multiply, source, destination);
    try std.testing.expectApproxEqAbs(0.16, multiply[0], 0.000001);
    try std.testing.expectApproxEqAbs(1.0, linearFogFactor(0.0, 10.0, 100.0), 0.000001);
    try std.testing.expectApproxEqAbs(0.5, linearFogFactor(55.0, 10.0, 100.0), 0.000001);
    try std.testing.expectApproxEqAbs(0.0, linearFogFactor(120.0, 10.0, 100.0), 0.000001);
    try std.testing.expect(!specs[9].depth_write);
    try std.testing.expectEqual(BlendMode.additive, specs[9].blend);
}

test "lightmap and fixed-light fixtures" {
    const normal = lambert(.{ 0.0, 0.0, 2.0 }, .{ 0.0, 0.0, 1.0 });
    try std.testing.expectApproxEqAbs(1.0, normal, 0.000001);
    try std.testing.expectApproxEqAbs(0.0, lambert(.{ 0.0, 0.0, 0.0 }, .{ 0.0, 0.0, 1.0 }), 0.000001);
    const combined = lightmapCombine(.{ 0.8, 0.6, 0.4 }, .{ 0.5, 0.25, 1.0 }, false);
    try std.testing.expectApproxEqAbs(0.4, combined[0], 0.000001);
    try std.testing.expectApproxEqAbs(0.15, combined[1], 0.000001);
    const inverted = lightmapCombine(.{ 0.8, 0.6, 0.4 }, .{ 0.5, 0.25, 1.0 }, true);
    try std.testing.expectApproxEqAbs(0.0, inverted[2], 0.000001);
    try std.testing.expectApproxEqAbs(0.5, pointAttenuation(1.0, 10.0, 1.0, 1.0, 0.0), 0.000001);
    try std.testing.expectApproxEqAbs(0.0, pointAttenuation(11.0, 10.0, 1.0, 1.0, 0.0), 0.000001);
}

test "stencil and water special-effect fixtures" {
    try std.testing.expect(stencilEqual(3, 3));
    try std.testing.expect(!stencilEqual(2, 3));
    const uv = animateWaterUv(.{ 0.25, 0.5 }, .{ 0.1, -0.2 }, 2.5);
    try std.testing.expectApproxEqAbs(0.5, uv[0], 0.000001);
    try std.testing.expectApproxEqAbs(0.0, uv[1], 0.000001);
    for (specs) |spec| {
        if (spec.family == .stencil or spec.family == .shadow or spec.family == .water or spec.family == .special)
            try std.testing.expect(spec.shader_effect != .textured and spec.shader_effect != .untextured);
    }
}

test "only the flowing-water effect enables the stage-0 texture matrix" {
    // CGraphicsEngine sets D3DTTFF_COUNT2 for 303 and D3DTTFF_DISABLE for 304.
    try std.testing.expect(usesTextureTransform(303));
    try std.testing.expect(!usesTextureTransform(304));
    // Nothing else transforms its texcoords, so every other draw gets identity.
    try std.testing.expect(!usesTextureTransform(3));
    try std.testing.expect(!usesTextureTransform(101));
    try std.testing.expect(!usesTextureTransform(9999));
}

test "terrain effects declare the stage combines their D3D states describe" {
    try std.testing.expectEqual(Combine.modulate_second, combineFor(101));
    try std.testing.expectEqual(Combine.mask_alpha, combineFor(100));
    try std.testing.expectEqual(Combine.mask_alpha, combineFor(102));
    try std.testing.expectEqual(Combine.mask_alpha_test, combineFor(104));
    // 103 is the single-stage noise pass, and everything outside the terrain
    // family samples one texture.
    try std.testing.expectEqual(Combine.single, combineFor(103));
    try std.testing.expectEqual(Combine.single, combineFor(3));
    try std.testing.expectEqual(BlendMode.multiply, (find(103) orelse unreachable).blend);
    try std.testing.expectEqual(BlendMode.multiply, (find(104) orelse unreachable).blend);
    try std.testing.expectEqual(BlendMode.replace, (find(101) orelse unreachable).blend);
}
