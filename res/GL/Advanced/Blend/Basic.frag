#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D textureSampler;
uniform vec4 texColor;
void main()
{   
    if(texColor.a < 0.1){
        FragColor = texture(textureSampler, TexCoords);
    }else{
        FragColor = texColor; 
    }          
}