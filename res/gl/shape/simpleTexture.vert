#version 330 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 color;
layout (location = 2) in vec2 inTextureCoord;

out vec4 fragColor;
out vec2 texrtureCoord;
void main(){
    gl_Position = pos;
    fragColor = color;
    texrtureCoord = vec2(inTextureCoord.x, inTextureCoord.y);
}