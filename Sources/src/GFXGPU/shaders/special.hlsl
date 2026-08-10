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
    output.color = legacy_vertex_color(input.color) * g_color;
    output.uv = input.uv;
    return output;
}

SpecialVertexOutput vs_special_transform(SpecialVertexInput input) {
    SpecialVertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = legacy_vertex_color(input.color) * g_color;
    output.uv = input.uv + g_color.xy * g_color.w;
    return output;
}

float4 ps_special_video(SpecialVertexOutput input) : SV_Target0 {
    return g_texture0.Sample(g_sampler0, saturate(input.uv)) * input.color;
}

float4 ps_special_transform(SpecialVertexOutput input) : SV_Target0 {
    float4 texel = g_texture0.Sample(g_sampler0, input.uv);
    // Vehicle tracks are effect 20: D3DTSS_COLOROP is ADD of the texture and
    // the diffuse, with the alpha selected straight from the texture, and the
    // whole thing multiplied into the ground by SRCBLEND=DESTCOLOR. Modulating
    // instead of adding made the white gaps between the tread marks darken the
    // ground by the diffuse colour, so every trace quad laid a flat rectangle
    // over the terrain - obvious on snow, invisible on grass. Same form as the
    // textured shaders use.
    if (g_stage.x != 0.0f) return float4(saturate(texel.rgb + input.color.rgb), texel.a);
    return texel * input.color;
}

float4 ps_special_depth(SpecialVertexOutput input) : SV_Target0 {
    return input.color;
}
