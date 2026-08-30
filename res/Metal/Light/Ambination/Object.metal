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
    float4 aNormal [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float4 normal;
    float4 outColor;
};

vertex VertexOut Ambination_Object_vertex(VertexIn in [[stage_in]],
                                         constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.position = ubo.projection * ubo.view * ubo.model * in.pos;
    out.normal = in.aNormal;
    out.outColor = in.inColor;
    return out;
}

fragment float4 Ambination_Object_fragment(VertexOut in [[stage_in]],
                                           constant UniformBlock& ubo [[buffer(8)]]) {
    float ambientStrength = 0.2;
    float4 ambient = ambientStrength * ubo.vec4Pool[3];
    return ambient * ubo.vec4Pool[4];
}
