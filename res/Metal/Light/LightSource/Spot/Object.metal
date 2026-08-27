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

vertex VertexOut Spot_Object_vertex(VertexIn in [[stage_in]],
                                    constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.fragPos = ubo.model * in.pos;
    out.position = ubo.projection * ubo.view * out.fragPos;
    out.normal = ubo.model * in.aNormal;
    out.objectColor = in.inColor;
    out.textureCoord = in.inTextureCoord;
    return out;
}

fragment float4 Spot_Object_fragment(VertexOut in [[stage_in]],
                                      constant UniformBlock& ubo [[buffer(0)]],
                                      texture2d<half> diffuseMap [[texture(0)]],
                                      texture2d<half> specularMap [[texture(1)]]) {
    constexpr sampler s(mag_filter::linear, min_filter::linear);

    float3 norm = normalize(in.normal.rgb);
    float3 lightDir = normalize(ubo.lights[0].position.xyz - in.fragPos.rgb);
    float diff = max(dot(norm, lightDir), 0.0);

    float3 viewDir = normalize(ubo.vec4Pool[0].xyz - in.fragPos.rgb);
    float3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), ubo.floatPool[0]);

    float theta = dot(lightDir, normalize(-ubo.lights[0].direction.xyz));
    float epsilon = ubo.lights[0].params.w - ubo.lights[0].direction.w;
    float intensity = clamp((theta - ubo.lights[0].direction.w) / epsilon, 0.0, 1.0);

    float distance = length(ubo.lights[0].position.xyz - in.fragPos.rgb);
    float attenuation = 1.0 / (ubo.lights[0].params.x + ubo.lights[0].params.y * distance + ubo.lights[0].params.z * (distance * distance));

    float3 ambient = ubo.lights[0].ambient.xyz * diffuseMap.sample(s, in.textureCoord).rgb;
    float3 diffuse = ubo.lights[0].diffuse.xyz * diff * diffuseMap.sample(s, in.textureCoord).rgb;
    float3 specular = ubo.lights[0].specular.xyz * spec * specularMap.sample(s, in.textureCoord).rgb;

    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    float3 result = ambient + diffuse + specular;
    return float4(result, 1.0);
}
