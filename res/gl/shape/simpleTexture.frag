#version 330 core
in vec4 fragColor;
in vec2 textureCoord;

out vec4 color;
uniform sampler2D textureSampler;

void main(){
    color = texture(textureSampler, textureCoord) * fragColor;
}