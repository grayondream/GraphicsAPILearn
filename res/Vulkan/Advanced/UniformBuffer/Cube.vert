#version 450 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inTextureCoord;

out vec4 fragColor;

out vec2 textureCoord;
layout (std140) uniform Matrices{
    mat4 projection;
    mat4 view;
};

uniform mat4 model;


void main(){
    gl_Position = projection * view * model * pos;
    textureCoord = inTextureCoord;
    fragColor = inColor;
}