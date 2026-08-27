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
    float4 position_view;
    float3 normal;
};

vertex VertexOut GeometryNormalLine_vertex(VertexIn in [[stage_in]],
                                           constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.position_view = ubo.view * ubo.model * in.pos;
    float3x3 normalMat = float3x3(transpose(inverse(float3x3(ubo.view) * float3x3(ubo.model))));
    out.normal = normalize(float3(float4(normalMat * in.aNormal.rgb, 0.0)));
    out.position = ubo.projection * out.position_view;
    return out;
}

fragment half4 GeometryNormalLine_fragment(VertexOut in [[stage_in]]) {
    return half4(1.0h, 1.0h, 0.0h, 1.0h);
}