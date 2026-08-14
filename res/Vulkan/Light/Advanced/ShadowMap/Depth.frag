#version 450 core
out vec4 FragColor;

struct ULight { vec4 position; vec4 direction; vec4 ambient; vec4 diffuse; vec4 specular; vec4 params; };

layout(set=0, binding=0) uniform UniformBlock {
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

layout(set=0, binding=1) uniform sampler2D textureSampler;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

float LinearizeDepth(float depth){
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * floatPool[16] * floatPool[17]) / (floatPool[17] + floatPool[16] - z * (floatPool[17] - floatPool[16]));	
}

void main(){           
    float depthValue = texture(textureSampler, fs_in.TexCoords).r;
    // FragColor = vec4(vec3(LinearizeDepth(depthValue) / floatPool[17]), 1.0); // perspective
    FragColor = vec4(vec3(depthValue), 1.0); // orthographic
}