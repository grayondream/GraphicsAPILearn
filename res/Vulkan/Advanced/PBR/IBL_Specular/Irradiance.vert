#version 450 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 normal;

out vec3 WorldPos;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    WorldPos = pos.xyz;
    gl_Position =  projection * view * vec4(WorldPos, 1.0);
}