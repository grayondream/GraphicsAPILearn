#version 450 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 aNormal;
layout (location = 3) in vec2 inTextureCoord;

out vec2 textureCoord;
out vec4 fragColor;
out vec4 normal;
out vec4 position;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main(){
    gl_Position = projection * view * model * pos;
    textureCoord = inTextureCoord;
    fragColor = inColor;
    mat3 v3model = mat3(model);
    normal = vec4(mat3(transpose(inverse(v3model))) * aNormal.xyz, 1.0);
    position = vec4(v3model * pos.xyz, 1.0);
}