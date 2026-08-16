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

void main() {
    // 确保输出明显的边框颜色（例如红色）
    FragColor = vec4(1.0, 0.0, 0.0, 1.0); 
}