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

vertex VertexOut IBL_IC_Background_vertex(VertexIn in [[stage_in]],
                                          constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.WorldPos = in.pos.xyz;
    float4x4 rotView = float4x4(float3x3(ubo.view));
    float4 clipPos = ubo.projection * rotView * float4(out.WorldPos, 1.0);
    out.position = clipPos.xyww;
    return out;
}

fragment float4 IBL_IC_Background_fragment(VertexOut in [[stage_in]],
                                           constant UniformBlock& ubo [[buffer(0)]],
                                           texturecube<float> environmentMap [[texture(0)]],
                                           sampler smp [[sampler(0)]]) {
    float3 envColor = environmentMap.sample(smp, in.WorldPos).rgb;
    envColor = envColor / (envColor + float3(1.0));
    envColor = pow(envColor, float3(1.0/2.2));
    return float4(envColor, 1.0);
}