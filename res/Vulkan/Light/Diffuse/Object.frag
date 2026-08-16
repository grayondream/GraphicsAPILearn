#version 450 core
#extension GL_EXT_scalar_block_layout : require

out vec4 FragColor;

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
    ULight lights[256];
};

in vec4 normal;
in vec4 fragPos;
in vec4 objOriginColor;
void main(){
    // ambient
    float ambientStrength = 0.3;
    vec4 ambient = ambientStrength * vec4Pool[3];
  	
    // diffuse 
    vec4 norm = normalize(normal);
    vec4 lightDir = normalize(vec4Pool[2] - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 diffuse = diff * vec4Pool[3];
            
    vec4 result = (ambient + diffuse) * vec4Pool[4];
    FragColor = result;
}
