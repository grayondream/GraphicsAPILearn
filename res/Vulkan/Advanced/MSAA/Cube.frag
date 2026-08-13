#version 450 core
in vec4 fragColor;
in vec2 textureCoord;

out vec4 color;
layout(set=0, binding=1) uniform sampler2D textureSampler;

void main(){
    //color = fragColor;
    color = vec4(0.0, 1.0, 0.0, 1.0);
}