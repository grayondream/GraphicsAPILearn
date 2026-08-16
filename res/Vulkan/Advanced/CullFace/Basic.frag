#version 450 core
#extension GL_EXT_scalar_block_layout : require

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

out vec4 FragColor;

in vec2 TexCoords;

layout(set=0, binding=1) uniform sampler2D textureSampler;
void main()
{   
    if(vec4Pool[6].a < 0.1){
        FragColor = texture(textureSampler, TexCoords);
    }else{
        FragColor = vec4Pool[6];
    }
}