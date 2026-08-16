#version 450 core
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 textureCoord;

out vec4 color;
layout(set=0, binding=1) uniform sampler2D textureSampler;

void main(){
    color = texture(textureSampler, textureCoord);
}
