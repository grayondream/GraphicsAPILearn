#version 450 core
out vec4 FragColor;

in vec3 TexCoords;

layout(set=0, binding=1) uniform samplerCube skybox;

void main()
{    
    FragColor = texture(skybox, TexCoords);
}