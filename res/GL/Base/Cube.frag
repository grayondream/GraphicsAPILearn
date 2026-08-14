#version 430 core
in vec4 fragColor;
in vec2 textureCoord;

out vec4 color;
layout(binding = 0) uniform sampler2D textureSampler;

void main(){
    //color = fragColor;
    color = texture(textureSampler, textureCoord);
}
