#include "bindings.hlsl"

struct VertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

VertexOutput vs_textured(VertexInput input) {
    VertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = input.color * g_color;
    output.uv = input.uv;
    return output;
}

float4 ps_textured(VertexOutput input) : SV_Target0 {
    return g_texture0.Sample(g_sampler0, input.uv) * input.color;
}
