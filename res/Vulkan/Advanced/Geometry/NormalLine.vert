#version 450 core
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 aNormal;

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

const float MAGNITUDE = 0.4;

out VS_OUT {
    vec3 normal;
    vec3 clipOffset;
} vs_out;

void main(){
    gl_Position = projection * view * model * pos;
    mat3 normalMatrix = mat3(transpose(inverse(mat3(view) * mat3(model))));
    vec3 n = normalize(vec3(vec4(normalMatrix * aNormal.rgb, 0.0)));
    vs_out.normal = n;
    vs_out.clipOffset = vec3(projection * vec4(n * MAGNITUDE, 0.0));
}
