#version 450 core
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aNormal;

struct ULight { vec4 position; vec4 direction; vec4 ambient; vec4 diffuse; vec4 specular; vec4 params; };

layout(set=0, binding=0, std430) uniform UniformBlock {
    mat4 projection;
    mat4 view;
    mat4 model;
    mat4 normalMatrix;
    mat4 viewModel;
    mat4 extraMat4[14];
    vec4 vec4Pool[64];
    vec4 vec3Pool[64];
    float floatPool[64];
    ULight lights[2];
};

void main()
{
    gl_Position = extraMat4[0] * model * aPos;
    // VK 深度缓冲范围 [0,1]：把 GL 风格投影矩阵（clip z ∈ [-1,1]）的 z 映射到
    // [0,1]（GL 由硬件自动做此映射，VK 必须手动）。否则 depth pass 写入的深度
    // 与主 pass ShadowCalculation 的 currentDepth(projCoords.z) 值域不一致，
    // 阴影比较错位（阴影左移/错乱）。
    gl_Position.z = gl_Position.z * 0.5 + 0.5 * gl_Position.w;
}