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
    output.uv = input.uv;
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
    output.uv = input.uv;
    return output;
}

float4 ps_textured(VertexOutput input) : SV_Target0 {
    return g_texture0.Sample(g_sampler0, input.uv) * input.color;
}
