#include "bindings.hlsl"

struct SpecialVertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct SpecialVertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

SpecialVertexOutput vs_special_video(SpecialVertexInput input) {
    SpecialVertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = input.color * g_color;
    output.uv = input.uv;
    return output;
}

SpecialVertexOutput vs_special_transform(SpecialVertexInput input) {
    SpecialVertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = input.color * g_color;
    output.uv = input.uv + g_color.xy * g_color.w;
    return output;
}

float4 ps_special_video(SpecialVertexOutput input) : SV_Target0 {
    return g_texture0.Sample(g_sampler0, saturate(input.uv)) * input.color;
}

float4 ps_special_transform(SpecialVertexOutput input) : SV_Target0 {
    return g_texture0.Sample(g_sampler0, input.uv) * input.color;
}

float4 ps_special_depth(SpecialVertexOutput input) : SV_Target0 {
    return input.color;
}
