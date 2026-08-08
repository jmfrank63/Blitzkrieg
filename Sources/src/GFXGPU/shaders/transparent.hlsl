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
    float3 fog_color : TEXCOORD2;
    float fog_range : TEXCOORD3;
    float alpha_threshold : TEXCOORD4;
};

TransparentVertexOutput vs_transparent(TransparentVertexInput input) {
    TransparentVertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = legacy_vertex_color(input.color) * g_color;
    output.uv = input.uv;
    output.view_depth = output.position.z / max(output.position.w, 0.0001f);
    output.fog_color = g_fog.xyz;
    output.fog_range = g_fog.w;
    output.alpha_threshold = g_color.a;
    return output;
}

float4 transparentColor(TransparentVertexOutput input) {
    float4 color = g_texture0.Sample(g_sampler0, input.uv) * input.color;
    color.rgb = apply_linear_fog(color.rgb, input.view_depth, input.fog_color, float2(0.0f, input.fog_range));
    return color;
}

float4 ps_transparent(TransparentVertexOutput input) : SV_Target0 {
    return transparentColor(input);
}

float4 ps_transparent_alpha_test(TransparentVertexOutput input) : SV_Target0 {
    float4 color = transparentColor(input);
    clip(color.a - input.alpha_threshold);
    return color;
}

float4 ps_transparent_additive(TransparentVertexOutput input) : SV_Target0 {
    return transparentColor(input);
}

float4 ps_transparent_multiply(TransparentVertexOutput input) : SV_Target0 {
    return transparentColor(input);
}
