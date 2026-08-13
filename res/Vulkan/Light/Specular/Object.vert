#version 450 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec4 normal;
out vec4 fragPos;
out vec4 objectColor;
void main(){
    fragPos = model * pos;
    gl_Position = projection * view * fragPos;
    normal = mat4(transpose(inverse(model))) * aNormal;
    objectColor = inColor;
}