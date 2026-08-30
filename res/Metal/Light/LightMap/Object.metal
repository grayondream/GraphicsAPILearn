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
    float2 inTextureCoord [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float4 normal;
    float4 fragPos;
    float4 objectColor;
    float2 textureCoord;
};

vertex VertexOut LightMap_Object_vertex(VertexIn in [[stage_in]],
                                        constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.fragPos = ubo.model * in.pos;
    out.position = ubo.projection * ubo.view * out.fragPos;
    out.normal = ubo.model * in.aNormal;
    out.objectColor = in.inColor;
    out.textureCoord = in.inTextureCoord;
    return out;
}

fragment float4 LightMap_Object_fragment(VertexOut in [[stage_in]],
                                          constant UniformBlock& ubo [[buffer(8)]],
                                          texture2d<float> diffuseMap [[texture(0)]],
                                          texture2d<float> specularMap [[texture(1)]]) {
    constexpr sampler s(mag_filter::linear, min_filter::linear);

    float4 ambient = ubo.lights[0].ambient * float4(diffuseMap.sample(s, in.textureCoord).rgb, 1.0);

    float4 norm = normalize(in.normal);
    float4 lightDir = normalize(ubo.lights[0].position - in.fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    float4 diffuse = ubo.lights[0].diffuse * diff * float4(diffuseMap.sample(s, in.textureCoord).rgb, 1.0);

    float4 viewDir = normalize(ubo.vec4Pool[0] - in.fragPos);
    float4 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), ubo.floatPool[0]);
    float4 specular = ubo.lights[0].specular * spec * float4(specularMap.sample(s, in.textureCoord).rgb, 1.0);

    float4 result = ambient + diffuse + specular;
    return result;
}
