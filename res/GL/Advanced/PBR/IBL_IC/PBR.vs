#version 430 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 normal;

struct ULight { vec4 position; vec4 direction; vec4 ambient; vec4 diffuse; vec4 specular; vec4 params; };

layout(binding = 0) uniform UniformBlock {
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

out vec2 TexCoords;
out vec3 WorldPos;
out vec3 Normal;

void main()
{
    TexCoords = aTexCoords;
    WorldPos = vec3(model * vec4(pos));
    Normal = mat3(normalMatrix) * normal.xyz;   

    gl_Position =  projection * view * vec4(WorldPos, 1.0);
}
