#include "bindings.hlsl"

struct UiVertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct UiVertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

UiVertexOutput vs_ui(UiVertexInput input) {
    UiVertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = input.color * g_color;
    output.uv = input.uv;
    return output;
}

float4 ps_ui(UiVertexOutput input) : SV_Target0 {
    return g_texture0.Sample(g_sampler0, input.uv) * input.color;
}

float4 ps_ui_alpha_test(UiVertexOutput input) : SV_Target0 {
    float4 color = g_texture0.Sample(g_sampler0, input.uv) * input.color;
    clip(color.a - g_color.a);
    return color;
}
