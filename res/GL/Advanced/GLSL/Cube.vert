#version 330 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inTextureCoord;

out vec2 textureCoord;
out vec4 fragColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform bool enablePointSize;
uniform bool enableVertexId;

void main(){
    gl_Position = projection * view * model * pos;
    textureCoord = inTextureCoord;
    if(enablePointSize){
        fragColor = vec4(0.0, 1.0, 1.0, 1.0);
        gl_PointSize = gl_Position.z * 10.0;
    }else{
        fragColor = inColor;
    }

    if(enableVertexId){
        fragColor = vec4(gl_VertexID % 2, gl_VertexID % 3, gl_VertexID % 4, 1.0);
    }
    
    
}