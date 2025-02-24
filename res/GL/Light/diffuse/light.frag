#version 330 core

out vec4 color;

uniform vec4 objectColor;

void main(){
    //color = fragColor;
    color = vec4(1.0);//objectColor;
}