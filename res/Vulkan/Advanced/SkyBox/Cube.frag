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

in vec4 fragColor;
in vec2 textureCoord;

out vec4 color;
layout(set=0, binding=1) uniform sampler2D textureSampler;
layout(set=0, binding=2) uniform samplerCube skyBoxSampler;
in vec4 normal;
in vec4 position;

void main(){
    //color = fragColor;
    if(floatPool[36] > 0.5){
        vec3 I = normalize(position.xyz - vec4Pool[1].xyz);
        vec3 R = reflect(I, normalize(normal.xyz));
        color = vec4(texture(skyBoxSampler, R).rgb, 1.0);
        return;
    }else if(floatPool[37] > 0.5){
        float ratio = 1.00 / 1.52;
        vec3 I = normalize(position.xyz - vec4Pool[1].xyz);
        vec3 R = refract(I, normalize(normal.xyz), ratio);
        color = vec4(texture(skyBoxSampler, R).rgb, 1.0);    
        return;
    }
    
    color = texture(textureSampler, textureCoord);
}