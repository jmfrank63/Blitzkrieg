#include "bindings.hlsl"

struct UnlitVertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct UnlitVertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float alpha_threshold : TEXCOORD3;
};

UnlitVertexOutput vs_unlit(UnlitVertexInput input) {
    UnlitVertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = input.color * g_color;
    output.uv = input.uv;
    output.alpha_threshold = g_color.a;
    return output;
}

float4 ps_unlit(UnlitVertexOutput input) : SV_Target0 {
    return input.color;
}

float4 ps_unlit_textured(UnlitVertexOutput input) : SV_Target0 {
    return g_texture0.Sample(g_sampler0, input.uv) * input.color;
}

float4 ps_alpha_test(UnlitVertexOutput input) : SV_Target0 {
    float4 color = g_texture0.Sample(g_sampler0, input.uv) * input.color;
    clip(color.a - input.alpha_threshold);
    return color;
}
