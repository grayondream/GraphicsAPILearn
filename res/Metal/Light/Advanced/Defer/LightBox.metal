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
};

vertex VertexOut Defer_LightBox_vertex(VertexIn in [[stage_in]],
                                       constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.position = ubo.projection * ubo.view * ubo.model * float4(in.pos);
    return out;
}

fragment float4 Defer_LightBox_fragment(VertexOut in [[stage_in]],
                                        constant UniformBlock& ubo [[buffer(0)]]) {
    return float4(ubo.vec4Pool[3].rgb, 1.0);   // lightColor
}