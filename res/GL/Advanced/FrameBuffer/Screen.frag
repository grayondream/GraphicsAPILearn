#version 330 core
in vec2 textureCoord;
in vec4 color;
out vec4 ocolor;
uniform sampler2D textureSampler;

void main(){
    ocolor = texture(textureSampler, textureCoord); 
}