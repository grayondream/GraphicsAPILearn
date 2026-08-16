#version 450 core
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;

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

void main(){
    gl_Position = extraMat4[0] * model * pos;
}