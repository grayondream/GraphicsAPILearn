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
};

struct VertexOut {
    float4 position [[position]];
    float3 TexCoords;
};

vertex VertexOut SkyBox_vertex(VertexIn in [[stage_in]],
                               constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.TexCoords = in.aPos;
    float4 pos = ubo.projection * ubo.view * float4(in.aPos, 1.0);
    out.position = pos.xyww;
    return out;
}

fragment half4 SkyBox_fragment(VertexOut in [[stage_in]],
                               texturecube<float> skybox [[texture(1)]]) {
    constexpr sampler s(mag_filter::linear, min_filter::linear);
    return half4(skybox.sample(s, in.TexCoords));
}