#version 430 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 normal;
layout (location = 3) in vec2 aOffset;

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

out vec4 color;
void main(){
    int instanceIndex = gl_InstanceID;
    float c = instanceIndex * 5.0 / 255;
    color = vec4(c, 0.0, 0.0, 1.0); // Red color

    vec4 position = pos * (gl_InstanceID / 100.0) + vec4(aOffset, 0.0, 1.0);
    gl_Position = projection * view * model * position;
}