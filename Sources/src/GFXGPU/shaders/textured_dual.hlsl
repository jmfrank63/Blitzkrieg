#include "bindings.hlsl"

// The multitextured terrain passes. Fixed-function D3D combined two texture
// stages per draw; g_screen.w selects which combination this draw wants, matching
// effects.zig's Combine and the D3DTSS_* states in CGraphicsEngine.
#define COMBINE_MODULATE_SECOND 1.0f
#define COMBINE_MASK_ALPHA      2.0f
#define COMBINE_MASK_ALPHA_TEST 3.0f

// D3DRS_ALPHAREF 50 with D3DCMP_GREATEREQUAL.
#define ALPHA_REFERENCE (50.0f / 255.0f)

struct VertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

VertexOutput vs_textured_dual(VertexInput input) {
    VertexOutput output;
    output.position = transform_legacy_position(input.position);
    output.color = legacy_vertex_color(input.color) * g_color;
    output.uv = input.uv;
    output.uv1 = input.uv1;
    return output;
}

float4 ps_textured_dual(VertexOutput input) : SV_Target0 {
    float4 base = g_texture0.Sample(g_sampler0, input.uv);
    float4 second = g_texture1.Sample(g_sampler1, input.uv1);
    float mode = g_screen.w;

    if (mode >= COMBINE_MASK_ALPHA_TEST) {
        // Effect 104: the noise carries the colour and the crosset the mask, and
        // the fragment is dropped where the cross is not covering. SDL_GPU has no
        // fixed-function alpha test, so it is a discard.
        clip(second.a - ALPHA_REFERENCE);
        return float4(base.rgb, second.a);
    }
    if (mode >= COMBINE_MASK_ALPHA) {
        // Effects 100 and 102: tileset shaded by the vertex colour, masked by the
        // crosset's alpha so one ground type fades into the next.
        return float4(base.rgb * input.color.rgb, second.a);
    }
    // Effect 101: the noise modulates the shaded tileset; alpha stays the base's.
    return float4(base.rgb * input.color.rgb * second.rgb, base.a * input.color.a);
}
