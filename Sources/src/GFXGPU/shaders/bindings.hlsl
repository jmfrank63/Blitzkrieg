#include "common.hlsl"

cbuffer FrameUniforms : register(b0, space1) { float4x4 g_view_proj; float4 g_fog; };
// g_screen is (pre_transformed, 1/viewport_width, 1/viewport_height, 0).
cbuffer DrawUniforms : register(b1, space1) { float4x4 g_world; float4 g_color; float4 g_screen; float4x4 g_texture_matrix; };
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
// D3D9's stage-0 texture transform under D3DTTFF_COUNT2: the matrix is applied
// to the incoming coordinate and the first two components are kept. The river
// layers scroll by translating u through it once per frame, which is the whole
// of the water's motion -- with no matrix reaching the shader the water stood
// still. The renderer passes the identity for every draw whose effect does not
// enable the transform, so this is unconditional.
float2 transform_legacy_texcoord(float2 uv) {
    return mul(float4(uv, 1.0f, 1.0f), g_texture_matrix).xy;
}

float4 transform_legacy_position(float3 position) {
    if (g_screen.x == 0.0f) return mul(mul(float4(position, 1.0f), g_world), g_view_proj);
    return float4(position.x * g_screen.y * 2.0f - 1.0f, 1.0f - position.y * g_screen.z * 2.0f, position.z, 1.0f);
}
