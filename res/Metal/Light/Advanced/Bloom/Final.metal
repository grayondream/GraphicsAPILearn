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

vertex VertexOut Bloom_Final_vertex(VertexIn in [[stage_in]],
                                    constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.TexCoords = in.textureCoord;
    out.position = float4(in.pos, 1.0);
    return out;
}

fragment float4 Bloom_Final_fragment(VertexOut in [[stage_in]],
                                     constant UniformBlock& ubo [[buffer(8)]],
                                     texture2d<float> scene [[texture(0)]],
                                     texture2d<float> bloomBlur [[texture(1)]],
                                     sampler sceneSampler [[sampler(0)]],
                                     sampler bloomSampler [[sampler(1)]]) {
    const float gamma = 2.2;
    float3 hdrColor = scene.sample(sceneSampler, in.TexCoords).rgb;      
    float3 bloomColor = bloomBlur.sample(bloomSampler, in.TexCoords).rgb;
    if(ubo.floatPool[11] > 0.5)   // bloom
        hdrColor += bloomColor; // additive blending
    // tone mapping
    float3 result = float3(1.0) - exp(-hdrColor * ubo.floatPool[4]);   // exposure
    // also gamma correct while we're at it       
    result = pow(result, float3(1.0 / gamma));
    return float4(result, 1.0);
}