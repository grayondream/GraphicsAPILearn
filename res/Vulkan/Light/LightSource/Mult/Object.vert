#version 450 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 aNormal;
layout (location = 3) in vec2 inTextureCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec4 normal;
out vec4 FragPos;
out vec4 objectColor;
out vec2 TexCoords;
void main(){
    FragPos = model * pos;
    gl_Position = projection * view * FragPos;
    normal = mat4(transpose(inverse(model))) * aNormal;
    objectColor = inColor;
    TexCoords = inTextureCoord;
}