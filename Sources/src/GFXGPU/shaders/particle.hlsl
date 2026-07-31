#include "bindings.hlsl"

struct ParticleVertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct ParticleVertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float view_depth : TEXCOORD1;
};

// Particle quads are expanded by the legacy CPU path. This shader only
// transforms the supplied vertices, preventing a second expansion.
ParticleVertexOutput vs_particle(ParticleVertexInput input) {
    ParticleVertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), g_world), g_view_proj);
    output.color = input.color * g_color;
    output.uv = input.uv;
    output.view_depth = output.position.z / max(output.position.w, 0.0001f);
    return output;
}

float4 particleColor(ParticleVertexOutput input) {
    float4 color = g_texture0.Sample(g_sampler0, input.uv) * input.color;
    color.rgb = apply_linear_fog(color.rgb, input.view_depth, g_fog.xyz, float2(0.0f, g_fog.w));
    clip(color.a - g_color.a);
    return color;
}

float4 ps_particle_additive(ParticleVertexOutput input) : SV_Target0 {
    return particleColor(input);
}

float4 ps_particle_modulate(ParticleVertexOutput input) : SV_Target0 {
    return particleColor(input);
}
