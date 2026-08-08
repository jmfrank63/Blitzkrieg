#include "bindings.hlsl"

struct VertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
};

VertexOutput vs_untextured(VertexInput input) {
    VertexOutput output;
    output.position = transform_legacy_position(input.position);
    output.color = legacy_vertex_color(input.color) * g_color;
    return output;
}

// Vertex formats without GFXFVF_DIFFUSE have no colour to read: mesh vertices
// carry a normal where the lit formats keep their diffuse DWORD. Declaring a
// COLOR0 attribute over those bytes read the normal as a colour, which is what
// made meshes iridescent. These variants take the draw colour instead.
struct VertexInputNoColor {
    float3 position : POSITION0;
};

VertexOutput vs_untextured_nocolor(VertexInputNoColor input) {
    VertexOutput output;
    output.position = transform_legacy_position(input.position);
    output.color = g_color;
    return output;
}

float4 ps_untextured(float4 color : COLOR0) : SV_Target0 {
    return color;
}
