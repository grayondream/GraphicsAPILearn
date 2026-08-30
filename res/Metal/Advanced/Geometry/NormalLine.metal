#include <metal_stdlib>
using namespace metal;
// __MAT_HELPERS__ (auto-added: MSL lacks inverse()/mat4(mat3))
float3x3 mat3Inverse(float3x3 m) {
    float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2];
    float a10 = m[1][0], a11 = m[1][1], a12 = m[1][2];
    float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2];
    float b01 =  a22*a11 - a12*a21;
    float b11 = -a22*a10 + a12*a20;
    float b21 =  a21*a10 - a11*a20;
    float det = a00*b01 + a01*b11 + a02*b21;
    float id = 1.0 / det;
    return float3x3(
        b01*id, (-a22*a01 + a02*a21)*id, ( a12*a01 - a02*a11)*id,
        b11*id, ( a22*a00 - a02*a20)*id, (-a12*a00 + a02*a10)*id,
        b21*id, (-a21*a00 + a01*a20)*id, ( a11*a00 - a01*a10)*id
    );
}
float4x4 mat4FromMat3(float3x3 m) {
    return float4x4(float4(m[0], 0.0), float4(m[1], 0.0), float4(m[2], 0.0), float4(0.0, 0.0, 0.0, 1.0));
}

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
                                           constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.position_view = ubo.view * ubo.model * in.pos;
    float3x3 normalMat = float3x3(transpose(mat3Inverse(float3x3(ubo.view[0].xyz, ubo.view[1].xyz, ubo.view[2].xyz) * float3x3(ubo.model[0].xyz, ubo.model[1].xyz, ubo.model[2].xyz))));
    out.normal = normalize(float3(float4(normalMat * in.aNormal.rgb, 0.0)));
    out.position = ubo.projection * out.position_view;
    return out;
}

fragment half4 GeometryNormalLine_fragment(VertexOut in [[stage_in]]) {
    return half4(1.0h, 1.0h, 0.0h, 1.0h);
}