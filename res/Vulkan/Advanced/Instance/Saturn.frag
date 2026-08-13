#version 450 core
out vec4 FragColor;

in vec2 TexCoords;

layout(set=0, binding=1) uniform sampler2D texture_diffuse1;

void main()
{    
    FragColor = texture(texture_diffuse1, TexCoords);
}