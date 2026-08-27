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
    float2 inTextureCoord [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 textureCoord;
    float4 fragColor;
};

vertex VertexOut DepthTest_vertex(VertexIn in [[stage_in]],
                                  constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.position = ubo.projection * ubo.view * ubo.model * in.pos;
    out.textureCoord = in.inTextureCoord;
    out.fragColor = in.inColor;
    return out;
}

fragment half4 DepthTest_fragment(VertexOut in [[stage_in]]) {
    float near = 0.1;
    float far = 100.0;
    float z = in.position.z * 2.0 - 1.0;
    float depth = (2.0 * near * far) / (far + near - z * (far - near));
    depth = depth / far;
    return half4(half3(depth), 1.0h);
}