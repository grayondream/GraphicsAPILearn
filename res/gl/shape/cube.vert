#version 330 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inTextureCoord;

//out vec2 textureCoord;
out vec4 fragColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main(){
    gl_Position = projection * view * model * pos;
    //textureCoord = inTextureCoord;
    fragColor = inColor;
}