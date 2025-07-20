#version 330 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 textureCoord;
layout (location = 3) in vec4 normal;

vec2 TexCoords;

void main(){
    TexCoords = textureCoord;
    gl_Position = pos;
}