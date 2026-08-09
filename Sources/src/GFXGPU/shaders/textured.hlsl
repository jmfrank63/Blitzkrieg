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
    output.position = transform_legacy_position(input.position);
    output.color = legacy_vertex_color(input.color) * g_color;
    output.uv = transform_legacy_texcoord(input.uv);
    return output;
}

// See untextured.hlsl: formats without GFXFVF_DIFFUSE substitute the draw
// colour. The texcoord is the second declared input here, so the pipeline binds
// it to location 1 rather than 2.
struct VertexInputNoColor {
    float3 position : POSITION0;
    float2 uv : TEXCOORD0;
};

VertexOutput vs_textured_nocolor(VertexInputNoColor input) {
    VertexOutput output;
    output.position = transform_legacy_position(input.position);
    output.color = g_color;
    output.uv = transform_legacy_texcoord(input.uv);
    return output;
}

float4 ps_textured(VertexOutput input) : SV_Target0 {
    float4 texel = g_texture0.Sample(g_sampler0, input.uv);
    // D3DTOP_ADD at stage 0, which only the vehicle tracks use. Alpha stays
    // D3DTOP_SELECTARG1 over the texture there, so it is not combined.
    if (g_stage.x != 0.0f) return float4(saturate(texel.rgb + input.color.rgb), texel.a);
    float4 result = texel * input.color;
    // D3DCMP_GREATEREQUAL against D3DRS_ALPHAREF. The cutout has to happen
    // before the depth write, or a sprite's transparent corners occlude what is
    // behind them.
    clip(g_stage.y > 0.0f ? result.a - g_stage.y : 1.0f);
    return result;
}
