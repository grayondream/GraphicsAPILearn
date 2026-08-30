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
    float2 aTexCoords [[attribute(2)]];
    float4 normal [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 WorldPos;
};

vertex VertexOut IBL_IC_CUBE_vertex(VertexIn in [[stage_in]],
                                    constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.WorldPos = in.pos.xyz;
    out.position = ubo.projection * ubo.view * float4(out.WorldPos, 1.0);
    return out;
}

fragment float4 IBL_IC_CUBE_fragment(VertexOut in [[stage_in]],
                                     constant UniformBlock& ubo [[buffer(8)]],
                                     texture2d<float> equirectangularMap [[texture(0)]],
                                     sampler smp [[sampler(0)]]) {
    const float2 invAtan = float2(0.1591, 0.3183);
    float3 normPos = normalize(in.WorldPos);
    float2 uv = float2(atan2(normPos.z, normPos.x), asin(normPos.y));
    uv *= invAtan;
    uv += 0.5;
    float3 color = equirectangularMap.sample(smp, uv).rgb;
    return float4(color, 1.0);
}