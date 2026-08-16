#version 450 core
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inTextureCoord;

struct ULight { vec4 position; vec4 direction; vec4 ambient; vec4 diffuse; vec4 specular; vec4 params; };

layout(set=0, binding=0, std430) uniform UniformBlock {
    mat4 projection;
    mat4 view;
    mat4 model;
    mat4 normalMatrix;
    mat4 viewModel;
    mat4 extraMat4[14];
    vec4 vec4Pool[64];
    vec4 vec3Pool[64];
    float floatPool[64];
    ULight lights[1];
};

out vec4 fragColor;

out vec2 textureCoord;
void main(){
    gl_Position = projection * view * model * pos;
    textureCoord = inTextureCoord;
    if(floatPool[43] > 0.5){
        fragColor = vec4(0.0, 1.0, 1.0, 1.0);
        gl_PointSize = gl_Position.z * 10.0;
    }else{
        fragColor = inColor;
    }

    if(floatPool[42] > 0.5){
        fragColor = vec4(gl_VertexIndex % 2, gl_VertexIndex % 3, gl_VertexIndex % 4, 1.0);
    }
    
    
}