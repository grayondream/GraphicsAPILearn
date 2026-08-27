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
    float2 textureCoord [[attribute(2)]];
    float4 normal [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 FragPos;
    float3 Normal;
    float2 TexCoords;
};

vertex VertexOut ShadowMap_Depth_vertex(VertexIn in [[stage_in]],
                                        constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.FragPos = in.pos.xyz;
    out.Normal = in.normal.xyz;
    out.TexCoords = in.textureCoord;
    out.position = in.pos;
    return out;
}

fragment float4 ShadowMap_Depth_fragment(VertexOut in [[stage_in]],
                                         constant UniformBlock& ubo [[buffer(0)]],
                                         texture2d<float> textureSampler [[texture(0)]],
                                         sampler sampler2D [[sampler(0)]]) {
    float depthValue = textureSampler.sample(sampler2D, in.TexCoords).r;
    // FragColor = vec4(vec3(LinearizeDepth(depthValue) / floatPool[17]), 1.0); // perspective
    return float4(float3(depthValue), 1.0); // orthographic
}