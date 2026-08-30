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
    float4 normal [[attribute(2)]];
    float2 aOffset [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

vertex VertexOut Instance_Sphere_vertex(VertexIn in [[stage_in]],
                                        constant UniformBlock& ubo [[buffer(8)]],
                                        uint instanceID [[instance_id]]) {
    VertexOut out;
    float c = instanceID * 5.0 / 255;
    out.color = float4(c, 0.0, 0.0, 1.0);
    
    float4 position = in.pos * (instanceID / 100.0) + float4(in.aOffset, 0.0, 1.0);
    out.position = ubo.projection * ubo.view * ubo.model * position;
    return out;
}

fragment float4 Instance_Sphere_fragment(VertexOut in [[stage_in]]) {
    return in.color;
}