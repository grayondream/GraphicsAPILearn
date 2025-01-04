#version 430 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 color;

out vec4 fragColor;
void main(){
    gl_Position = pos;
    fragColor = color;
}