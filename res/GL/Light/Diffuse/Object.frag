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
    // TEMP PROBE2: 4D vs 3D side by side + w diagnostics
    vec4 d4 = vec4Pool[2] - fragPos;
    float dOrig = max(dot(normalize(normal), normalize(d4)), 0.0);
    float dProbe = clamp(dot(normalize(normal.xyz), normalize(d4.xyz)), 0.0, 1.0);
    FragColor = vec4(dOrig, dProbe, clamp(abs(fragPos.w) * 0.25 + abs(d4.w) * 0.25, 0.0, 1.0), 1.0);
}
