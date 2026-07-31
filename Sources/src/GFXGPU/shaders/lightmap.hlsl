#include "bindings.hlsl"

struct LightmapVertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

struct LightmapVertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

LightmapVertexOutput vs_lightmap(LightmapVertexInput input) {
    LightmapVertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = input.color * g_color;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    return output;
}

float4 ps_lightmap_modulate(LightmapVertexOutput input) : SV_Target0 {
    return g_texture0.Sample(g_sampler0, input.uv0) * g_texture1.Sample(g_sampler1, input.uv1) * input.color;
}

float4 ps_lightmap_complement(LightmapVertexOutput input) : SV_Target0 {
    float4 base = g_texture0.Sample(g_sampler0, input.uv0);
    float4 lightmap = g_texture1.Sample(g_sampler1, input.uv1);
    return base * (1.0f - lightmap) * input.color;
}
