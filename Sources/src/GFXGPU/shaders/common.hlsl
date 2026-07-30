struct FrameUniforms {
    float4x4 g_view_proj;
    float4 g_fog;
};

struct DrawUniforms {
    float4x4 g_world;
    float4 g_color;
};

struct LightUniforms {
    float4 g_light_data[32];
};
