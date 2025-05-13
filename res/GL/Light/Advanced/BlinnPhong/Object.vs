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

void main(){
    opos = pos;
    gl_Position = projection * view * model * pos;
    TexCoord = textureCoord;
    Normal = normal;
}