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
    float4 aPos [[attribute(0)]];
    float4 aColor [[attribute(1)]];
    float2 aTexCoord [[attribute(2)]];
    float4 aNormal [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 textureCoord;
    float4 fragColor;
};

vertex VertexOut Cube_vertex(VertexIn in [[stage_in]],
                             constant UniformBlock& ub [[buffer(8)]]) {
    VertexOut out;
    out.position = ub.projection * ub.view * ub.model * in.aPos;
    out.textureCoord = in.aTexCoord;
    out.fragColor = in.aColor;
    return out;
}

fragment half4 Cube_fragment(VertexOut in [[stage_in]],
                             texture2d<half> textureSampler [[texture(0)]]) {
    constexpr sampler s(mag_filter::linear, min_filter::linear);
    return textureSampler.sample(s, in.textureCoord);
}
