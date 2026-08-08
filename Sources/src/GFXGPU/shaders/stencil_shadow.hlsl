#include "bindings.hlsl"

struct StencilVertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct StencilVertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float alpha_threshold : TEXCOORD3;
};

StencilVertexOutput vs_stencil(StencilVertexInput input) {
    StencilVertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = legacy_vertex_color(input.color) * g_color;
    output.uv = input.uv;
    output.alpha_threshold = g_color.a;
    return output;
}

float4 ps_stencil_write(StencilVertexOutput input) : SV_Target0 { return input.color; }
float4 ps_stencil_test(StencilVertexOutput input) : SV_Target0 { return input.color; }

float4 ps_shadow_sprite(StencilVertexOutput input) : SV_Target0 {
    float4 color = g_texture0.Sample(g_sampler0, input.uv) * input.color;
    clip(color.a - input.alpha_threshold);
    return color;
}

float4 ps_shadow_mesh(StencilVertexOutput input) : SV_Target0 {
    return g_texture0.Sample(g_sampler0, input.uv) * input.color;
}
