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
    float3 pos [[attribute(0)]];
    float2 textureCoord [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 TexCoords;
};

vertex VertexOut Shadow_DebugQuand_vertex(VertexIn in [[stage_in]],
                                          constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.TexCoords = in.textureCoord;
    out.position = float4(in.pos, 1.0);
    return out;
}

fragment float4 Shadow_DebugQuand_fragment(VertexOut in [[stage_in]],
                                           constant UniformBlock& ubo [[buffer(0)]],
                                           texture2d<float> depthMap [[texture(0)]],
                                           sampler depthSampler [[sampler(0)]]) {
    float depthValue = depthMap.sample(depthSampler, in.TexCoords).r;
    // FragColor = vec4(vec3(LinearizeDepth(depthValue) / floatPool[17]), 1.0); // perspective
    return float4(float3(depthValue), 1.0); // orthographic
}