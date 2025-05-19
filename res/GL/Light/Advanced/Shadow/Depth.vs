#version 330 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 textureCoord;
layout (location = 3) in vec4 normal;


out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} vs_out;

void main(){
    vs_out.FragPos = pos.xyz;
    vs_out.Normal = normal.xyz;
    vs_out.TexCoords = textureCoord;
    gl_Position = pos;
}