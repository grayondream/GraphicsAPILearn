#version 330 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 textureCoord;
layout (location = 3) in vec4 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoord;
out vec4 opos;
out vec4 Normal;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} vs_out;

void main(){
    vs_out.FragPos = pos.xyz;
    vs_out.Normal = normal.xyz;
    vs_out.TexCoords = textureCoord;
    gl_Position = projection * view * pos;
}