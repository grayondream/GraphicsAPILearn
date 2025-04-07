#version 330 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inTextureCoord;

out vec2 textureCoord;


void main(){
    gl_Position = pos;
    textureCoord = inTextureCoord;
}