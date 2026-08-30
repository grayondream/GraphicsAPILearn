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
    float3 aPos [[attribute(0)]];
    float3 aNormal [[attribute(1)]];
    float2 aTexCoords [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 TexCoords;
};

vertex VertexOut model_vertex(VertexIn in [[stage_in]],
                              constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.TexCoords = in.aTexCoords;
    out.position = ubo.projection * ubo.view * ubo.model * float4(in.aPos, 1.0);
    return out;
}

fragment float4 model_fragment(VertexOut in [[stage_in]],
                               constant UniformBlock& ubo [[buffer(8)]],
                               texture2d<float> texture_diffuse1 [[texture(0)]],
                               sampler smp [[sampler(0)]]) {
    return texture_diffuse1.sample(smp, in.TexCoords);
}