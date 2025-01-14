#version 330 core

out vec4 color;

uniform vec4 lightColor;

void main(){
    //color = fragColor;
    color = lightColor;
}