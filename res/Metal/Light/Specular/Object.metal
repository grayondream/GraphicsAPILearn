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
    float4 fragPos;
    float4 objectColor;
};

vertex VertexOut Specular_Object_vertex(VertexIn in [[stage_in]],
                                        constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.fragPos = ubo.model * in.pos;
    out.position = ubo.projection * ubo.view * out.fragPos;
    out.normal = ubo.model * in.aNormal;
    out.objectColor = in.inColor;
    return out;
}

fragment float4 Specular_Object_fragment(VertexOut in [[stage_in]],
                                          constant UniformBlock& ubo [[buffer(8)]]) {
    float4 ambient = ubo.floatPool[1] * ubo.vec4Pool[3];

    float4 norm = normalize(in.normal);
    float4 lightDir = normalize(ubo.vec4Pool[2] - in.fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    float4 diffuse = ubo.floatPool[3] * diff * ubo.vec4Pool[3];

    float4 viewDir = normalize(ubo.vec4Pool[0] - in.fragPos);
    float4 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), ubo.floatPool[10]);
    float4 specular = ubo.floatPool[2] * spec * ubo.vec4Pool[3];

    float4 result = (ambient + diffuse + specular) * ubo.vec4Pool[4];
    return result;
}
