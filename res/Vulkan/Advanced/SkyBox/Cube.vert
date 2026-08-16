#version 450 core
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 aNormal;
layout (location = 3) in vec2 inTextureCoord;

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
    ULight lights[1];
};

out vec4 fragColor;

out vec2 textureCoord;
out vec4 normal;
out vec4 position;

void main(){
    gl_Position = projection * view * model * pos;
    textureCoord = inTextureCoord;
    fragColor = inColor;
    mat3 v3model = mat3(model);
    normal = vec4(mat3(transpose(inverse(v3model))) * aNormal.xyz, 1.0);
    position = vec4(v3model * pos.xyz, 1.0);
}