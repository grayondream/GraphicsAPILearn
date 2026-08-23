#version 430 core
out vec4 FragColor;

struct ULight { vec4 position; vec4 direction; vec4 ambient; vec4 diffuse; vec4 specular; vec4 params; };

layout(binding = 0) uniform UniformBlock {
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
    // TEMP PROBE: explicit 3D normalize + 3D dot (same as DX probe)
    vec3 n3 = normalize(normal.xyz);
    vec3 dv3 = vec4Pool[2].xyz - fragPos.xyz;
    float diff = clamp(dot(n3, normalize(dv3)), 0.0, 1.0);
    FragColor = vec4(diff * vec4Pool[3].xyz, 1.0);
}
