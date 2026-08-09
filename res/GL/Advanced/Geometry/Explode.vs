#version 330 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out VS_OUT {
    vec4 color;
} vs_out;

void main(){
    vs_out.color = inColor;
    gl_Position = projection * view * model * pos;
}