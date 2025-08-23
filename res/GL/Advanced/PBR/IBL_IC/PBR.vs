#version 330 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 normal;

out vec2 TexCoords;
out vec3 WorldPos;
out vec3 Normal;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat3 normalMatrix;

void main()
{
    TexCoords = aTexCoords;
    WorldPos = vec3(model * vec4(pos));
    Normal = normalMatrix * normal.xyz;   

    gl_Position =  projection * view * vec4(WorldPos, 1.0);
}