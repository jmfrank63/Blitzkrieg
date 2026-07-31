#include "bindings.hlsl"

struct LightingVertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
};

struct LightingVertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float3 world_position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

LightingVertexOutput vs_lighting(LightingVertexInput input) {
    LightingVertexOutput output;
    float4 world_position = mul(float4(input.position, 1.0f), g_world);
    output.position = mul(world_position, g_view_proj);
    output.world_position = world_position.xyz;
    output.normal = safe_normalize(mul(input.normal, (float3x3)g_world));
    output.color = input.color * g_color;
    output.uv = input.uv;
    return output;
}

float3 evaluate_lighting(LightingVertexOutput input) {
    float3 normal = safe_normalize(input.normal);
    float3 result = 0.0f;
    [loop]
    for (uint light_index = 0; light_index < 8; ++light_index) {
        uint base = light_index * 4;
        float4 position_or_direction = g_light_data[base + 0];
        float4 diffuse_and_ambient = g_light_data[base + 1];
        float4 specular_and_range = g_light_data[base + 2];
        float4 attenuation = g_light_data[base + 3];
        float3 to_light = position_or_direction.xyz;
        float intensity = 1.0f;
        if (position_or_direction.w > 0.5f) {
            to_light -= input.world_position;
            float distance = length(to_light);
            if (distance > specular_and_range.w) continue;
            to_light = safe_normalize(to_light);
            intensity = 1.0f / max(attenuation.x + distance * attenuation.y + distance * distance * attenuation.z, 0.0001f);
        } else {
            to_light = safe_normalize(-to_light);
        }
        result += diffuse_and_ambient.rgb * diffuse_and_ambient.a;
        result += diffuse_and_ambient.rgb * max(dot(normal, to_light), 0.0f) * intensity;
    }
    return result;
}

float4 ps_lighting(LightingVertexOutput input) : SV_Target0 {
    float4 texture_color = g_texture0.Sample(g_sampler0, input.uv);
    float3 lit = evaluate_lighting(input);
    return float4(texture_color.rgb * input.color.rgb * (lit + 0.05f), texture_color.a * input.color.a);
}
