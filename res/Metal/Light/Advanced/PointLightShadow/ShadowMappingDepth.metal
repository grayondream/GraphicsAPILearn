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
    float4 FragPos;
};

vertex VertexOut PointLightShadow_ShadowMappingDepth_vertex(VertexIn in [[stage_in]],
                                                           constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    float4 worldPos = ubo.model * in.pos;
    out.FragPos = worldPos;
    out.position = ubo.extraMat4[1] * worldPos;   // 当前面的 shadow matrix（固定槽 extraMat4[1] = shadowMatrices[0]）
    return out;
}

fragment float4 PointLightShadow_ShadowMappingDepth_fragment(VertexOut in [[stage_in]],
                                                             constant UniformBlock& ubo [[buffer(8)]]) {
    // lightPos → vec4Pool[2].xyz, far_plane → floatPool[17]（与 ShadowMapping.fs 同槽）
    float lightDistance = length(in.FragPos.xyz - ubo.vec4Pool[2].xyz);
    
    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / ubo.floatPool[17];
    
    // write this as modified depth
    return float4(lightDistance, 0.0, 0.0, 1.0);
}