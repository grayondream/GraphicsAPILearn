#include <metal_stdlib>
using namespace metal;

struct ULight {
    float4 position;
    float4 direction;
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float4 params;
};

struct UniformBlock {
    float4x4 projection;
    float4x4 view;
    float4x4 model;
    float4x4 normalMatrix;
    float4x4 viewModel;
    float4x4 extraMat4[14];
    float4 vec4Pool[64];
    float4 vec3Pool[64];
    float floatPool[64];
    ULight lights[256];
};

struct VertexIn {
    float4 pos [[attribute(0)]];
    float4 inColor [[attribute(1)]];
    float2 aTexCoords [[attribute(2)]];
    float4 normal [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 WorldPos;
};

vertex VertexOut IBL_Specular_Irradiance_vertex(VertexIn in [[stage_in]],
                                                constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.WorldPos = in.pos.xyz;
    out.position = ubo.projection * ubo.view * float4(out.WorldPos, 1.0);
    return out;
}

fragment float4 IBL_Specular_Irradiance_fragment(VertexOut in [[stage_in]],
                                                 constant UniformBlock& ubo [[buffer(8)]],
                                                 texturecube<float> environmentMap [[texture(0)]],
                                                 sampler smp [[sampler(0)]]) {
    const float PI = 3.14159265359;
    float3 N = normalize(in.WorldPos);
    float3 irradiance = float3(0.0);
    
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));
    
    float sampleDelta = 0.025;
    float nrSamples = 0.0;
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            irradiance += environmentMap.sample(smp, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / nrSamples);
    
    return float4(irradiance, 1.0);
}