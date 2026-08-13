#version 450 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 aNormal;

out VS_OUT {
    vec3 normal;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main(){
    gl_Position = view * model * pos;
    mat3 normalMatrix = mat3(transpose(inverse(mat3(view) * mat3(model))));
    vs_out.normal = normalize(vec3(vec4(normalMatrix * aNormal.rgb, 0.0)));
}