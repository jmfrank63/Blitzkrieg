#include "bindings.hlsl"

struct WaterVertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

struct WaterVertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float alpha_threshold : TEXCOORD3;
};

WaterVertexOutput vs_water(WaterVertexInput input) {
    WaterVertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = legacy_vertex_color(input.color) * g_color;
    output.uv0 = input.uv0 + g_color.xy * g_color.w;
    output.uv1 = input.uv1 + g_color.yx * g_color.w;
    output.alpha_threshold = g_color.a;
    return output;
}

float4 ps_water(WaterVertexOutput input) : SV_Target0 {
    float4 base = g_texture0.Sample(g_sampler0, input.uv0);
    float4 overlay = g_texture1.Sample(g_sampler1, input.uv1);
    return base * overlay * input.color;
}

float4 ps_water_single(WaterVertexOutput input) : SV_Target0 {
    return g_texture0.Sample(g_sampler0, input.uv0) * input.color;
}

float4 ps_water_alpha_test(WaterVertexOutput input) : SV_Target0 {
    float4 color = ps_water(input);
    clip(color.a - input.alpha_threshold);
    return color;
}
