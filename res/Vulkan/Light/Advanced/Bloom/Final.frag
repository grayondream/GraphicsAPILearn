#version 450 core
#extension GL_EXT_scalar_block_layout : require

out vec4 FragColor;

in vec2 TexCoords;

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
    ULight lights[2];
};

layout(set=0, binding=1) uniform sampler2D scene;
layout(set=0, binding=2) uniform sampler2D bloomBlur;

void main()
{             
    const float gamma = 2.2;
    vec3 hdrColor = texture(scene, TexCoords).rgb;      
    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
    if(floatPool[11] > 0.5)   // bloom
        hdrColor += bloomColor; // additive blending
    // tone mapping
    vec3 result = vec3(1.0) - exp(-hdrColor * floatPool[4]);   // exposure
    // also gamma correct while we're at it       
    result = pow(result, vec3(1.0 / gamma));
    FragColor = vec4(result, 1.0);
}