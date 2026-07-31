struct FrameUniforms {
    row_major float4x4 g_view_proj;
    float4 g_fog;
};

struct DrawUniforms {
    row_major float4x4 g_world;
    float4 g_color;
};

struct LightUniforms {
    float4 g_light_data[32];
};

// D3D9's half-pixel convention expressed in clip space.
float4 apply_ui_half_pixel(float4 clip_position, float2 viewport_size) {
    clip_position.x -= clip_position.w / viewport_size.x;
    clip_position.y += clip_position.w / viewport_size.y;
    return clip_position;
}

float3 apply_linear_fog(float3 color, float view_depth, float3 fog_color, float2 fog_range) {
    float factor = saturate((fog_range.y - view_depth) / max(fog_range.y - fog_range.x, 0.0001f));
    return lerp(fog_color, color, factor);
}
