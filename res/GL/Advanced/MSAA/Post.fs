#version 330 core
in vec4 fragColor;
in vec2 textureCoord;

out vec4 color;
uniform sampler2D textureSampler;

void main(){
    vec3 col = texture(textureSampler, textureCoord).rgb;
    float grayscale = 0.2126 * col.r + 0.7152 * col.g + 0.0722 * col.b;
    color = vec4(vec3(grayscale), 1.0);
}