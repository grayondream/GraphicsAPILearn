#version 430 core
in vec4 fragColor;
in vec2 textureCoord;

out vec4 color;
layout(binding = 0) uniform sampler2D textureSampler;

void main(){
    vec3 col = texture(textureSampler, textureCoord).rgb;
    float grayscale = 0.2126 * col.r + 0.7152 * col.g + 0.0722 * col.b;
    color = vec4(vec3(grayscale), 1.0);
    //color = vec4(1.0, 0.0, 0.0, 1.0);
}