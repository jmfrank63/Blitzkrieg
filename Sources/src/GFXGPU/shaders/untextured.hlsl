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
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = input.color * g_color;
    return output;
}

float4 ps_untextured(float4 color : COLOR0) : SV_Target0 {
    return color;
}
