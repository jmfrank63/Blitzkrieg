#version 450

// Fixed-function 2D fragment shader — supports multiple effects:
//   Effect 3 : alpha test + modulate (standard menus)
//   Effect 15: diffuse only, no texture (colored UI panels)
//   Effect 16: additive modulate (additive flashes)
//   Effect  2: opaque pass-through (backgrounds)

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec4 vSpecular;
layout(location = 2) in vec2 vTexCoord;

layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D uTexture;

layout(push_constant) uniform FragPushConsts {
    float alpha_ref;
    int  use_texture;   // 1 = modulate texture * diffuse
    int  alpha_test;    // 1 = discard if tex.a < ref
} pc;

void main()
{
    vec4 diffuse = vColor;
    if (pc.use_texture == 1)
    {
        vec4 tex = texture(uTexture, vTexCoord);
        diffuse *= tex;
    }

    if (pc.alpha_test == 1 && diffuse.a < pc.alpha_ref)
        discard;

    FragColor = diffuse;
}
