#include "common.hlsl"

cbuffer FrameUniforms : register(b0, space1) { float4x4 g_view_proj; float4 g_fog; };
cbuffer DrawUniforms : register(b1, space1) { float4x4 g_world; float4 g_color; };
cbuffer LightUniforms : register(b2, space1) { float4 g_light_data[32]; };

Texture2D g_texture0 : register(t0, space2);
Texture2D g_texture1 : register(t1, space2);
SamplerState g_sampler0 : register(s0, space2);
SamplerState g_sampler1 : register(s1, space2);
