#version 330 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 normal;
layout (location = 3) in vec2 aOffset;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform int count;

out vec4 color;
void main(){
    int instanceIndex = gl_InstanceID;
    float c = instanceIndex * 5.0 / 255;
    color = vec4(c, 0.0, 0.0, 1.0); // Red color

    vec4 position = pos * (gl_InstanceID / 100.0) + vec4(aOffset, 0.0, 1.0);
    gl_Position = projection * view * model * position;
}