#version 450 core
layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aNormal;

uniform mat4 model;

void main()
{
    gl_Position =  model * aPos;
}