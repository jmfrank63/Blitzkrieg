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

// D3DRS_SPECULARENABLE adds the vertex specular colour after the texture
// stages, and its alpha takes no part. CGraphicsEngine::DrawRects turns it on
// for any batch carrying a non-black specular, which is how a blinking UI
// element flashes: CSimpleWindow::Draw puts the blink colour there.
struct VertexInputSpecular {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float4 specular : COLOR1;
};

struct VertexOutputSpecular {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float4 specular : COLOR1;
};

VertexOutputSpecular vs_untextured_specular(VertexInputSpecular input) {
    VertexOutputSpecular output;
    output.position = transform_legacy_position(input.position);
    output.color = legacy_vertex_color(input.color) * g_color;
    output.specular = legacy_vertex_color(input.specular);
    return output;
}

float4 ps_untextured_specular(VertexOutputSpecular input) : SV_Target0 {
    clip(g_stage.y > 0.0f ? input.color.a - g_stage.y : 1.0f);
    return float4(saturate(input.color.rgb + input.specular.rgb), input.color.a);
}

float4 ps_untextured(float4 color : COLOR0) : SV_Target0 {
    // Same D3DRS_ALPHAREF test as the textured path; here the alpha is the
    // vertex colour's, since there is no texture to take it from.
    clip(g_stage.y > 0.0f ? color.a - g_stage.y : 1.0f);
    return color;
}
