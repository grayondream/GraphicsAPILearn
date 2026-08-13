#version 450 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;

uniform mat4 model;
uniform mat4 lightSpaceMatrix;

void main(){
    gl_Position = lightSpaceMatrix * model * pos;
}