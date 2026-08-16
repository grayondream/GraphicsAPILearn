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

out vec4 FragPos;

void main()
{
    vec4 worldPos = model * aPos;
    FragPos = worldPos;
    gl_Position = extraMat4[1] * worldPos;   // 当前面的 shadow matrix（固定槽 extraMat4[1] = shadowMatrices[0]）
}
