#include "bindings.hlsl"

struct TransparentVertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct TransparentVertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float view_depth : TEXCOORD1;
};

TransparentVertexOutput vs_transparent(TransparentVertexInput input) {
    TransparentVertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = input.color * g_color;
    output.uv = input.uv;
    output.view_depth = output.position.z / max(output.position.w, 0.0001f);
    return output;
}

float4 transparentColor(TransparentVertexOutput input) {
    float4 color = g_texture0.Sample(g_sampler0, input.uv) * input.color;
    color.rgb = apply_linear_fog(color.rgb, input.view_depth, g_fog.xyz, float2(0.0f, g_fog.w));
    return color;
}

float4 ps_transparent(TransparentVertexOutput input) : SV_Target0 {
    return transparentColor(input);
}

float4 ps_transparent_alpha_test(TransparentVertexOutput input) : SV_Target0 {
    float4 color = transparentColor(input);
    clip(color.a - g_color.a);
    return color;
}

float4 ps_transparent_additive(TransparentVertexOutput input) : SV_Target0 {
    return transparentColor(input);
}

float4 ps_transparent_multiply(TransparentVertexOutput input) : SV_Target0 {
    return transparentColor(input);
}
