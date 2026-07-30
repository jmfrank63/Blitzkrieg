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
