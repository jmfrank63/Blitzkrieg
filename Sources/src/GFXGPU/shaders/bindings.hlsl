#include "common.hlsl"

cbuffer FrameUniforms : register(b0, space1) { float4x4 g_view_proj; float4 g_fog; };
// g_screen is (pre_transformed, 1/viewport_width, 1/viewport_height, 0).
cbuffer DrawUniforms : register(b1, space1) { float4x4 g_world; float4 g_color; float4 g_screen; };
cbuffer LightUniforms : register(b2, space1) { float4 g_light_data[32]; };

Texture2D g_texture0 : register(t0, space2);
Texture2D g_texture1 : register(t1, space2);
SamplerState g_sampler0 : register(s0, space2);
SamplerState g_sampler1 : register(s1, space2);

// D3D9 treats a GFXFVF_XYZRHW position as already being in screen pixels and
// skips the world/view/projection pipeline for it entirely. The engine relies on
// that: CTerrain::MovePatches lays its patches out at integer pixel offsets
// taken from the viewport matrix and never sets a screen-space transform of its
// own, so transforming those vertices projected the whole map into a thin skewed
// ribbon. Screen-space draws that do set a transform (war fog, rects) pass
// pixel coordinates too, so they take the same path.
float4 transform_legacy_position(float3 position) {
    if (g_screen.x == 0.0f) return mul(mul(float4(position, 1.0f), g_world), g_view_proj);
    return float4(position.x * g_screen.y * 2.0f - 1.0f, 1.0f - position.y * g_screen.z * 2.0f, position.z, 1.0f);
}
