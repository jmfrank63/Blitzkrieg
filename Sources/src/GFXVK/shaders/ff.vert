#version 450

// Fixed-function 2D vertex shader — matches SGFXLVertex layout:
//   float x, y, z;
//   DWORD color, specular;
//   float tu, tv;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec4 aSpecular;
layout(location = 3) in vec2 aTexCoord;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec4 vSpecular;
layout(location = 2) out vec2 vTexCoord;

layout(push_constant) uniform PushConsts {
    mat4 mvp;
} pc;

void main()
{
    gl_Position = pc.mvp * vec4(aPos, 1.0);
    vColor = aColor;
    vSpecular = aSpecular;
    vTexCoord = aTexCoord;
}
