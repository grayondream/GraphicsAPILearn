#version 450 core
out vec4 FragColor;

in vec2 TexCoords;

layout(set=0, binding=1) uniform sampler2D textureSampler;
uniform vec4 texColor;
void main()
{   
    if(texColor.a < 0.1){
        FragColor = texture(textureSampler, TexCoords);
    }else{
        FragColor = texColor; 
    }          
}