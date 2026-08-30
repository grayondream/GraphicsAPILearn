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
    float4 FragPos;
    float4 objectColor;
    float2 TexCoords;
};

vertex VertexOut Mult_Object_vertex(VertexIn in [[stage_in]],
                                    constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.FragPos = ubo.model * in.pos;
    out.position = ubo.projection * ubo.view * out.FragPos;
    out.normal = ubo.model * in.aNormal;
    out.objectColor = in.inColor;
    out.TexCoords = in.inTextureCoord;
    return out;
}

float3 CalcDirLight(ULight light, float3 normal, float3 viewDir,
                    texture2d<float> diffuseMap, texture2d<float> specularMap,
                    float2 texCoords, float shininess) {
    float3 lightDir = normalize(-light.direction.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

    constexpr sampler s(mag_filter::linear, min_filter::linear);
    float3 ambient = light.ambient.xyz * diffuseMap.sample(s, texCoords).rgb;
    float3 diffuse = light.diffuse.xyz * diff * diffuseMap.sample(s, texCoords).rgb;
    float3 specular = light.specular.xyz * spec * specularMap.sample(s, texCoords).rgb;
    return (ambient + diffuse + specular);
}

float3 CalcPointLight(ULight light, float3 normal, float3 fragPos, float3 viewDir,
                      texture2d<float> diffuseMap, texture2d<float> specularMap,
                      float2 texCoords, float shininess) {
    float3 lightDir = normalize(light.position.xyz - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.params.x + light.params.y * distance + light.params.z * (distance * distance));

    constexpr sampler s(mag_filter::linear, min_filter::linear);
    float3 ambient = light.ambient.xyz * diffuseMap.sample(s, texCoords).rgb;
    float3 diffuse = light.diffuse.xyz * diff * diffuseMap.sample(s, texCoords).rgb;
    float3 specular = light.specular.xyz * spec * specularMap.sample(s, texCoords).rgb;
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

float3 CalcSpotLight(ULight light, float3 normal, float3 fragPos, float3 viewDir,
                     texture2d<float> diffuseMap, texture2d<float> specularMap,
                     float2 texCoords, float shininess) {
    float3 lightDir = normalize(light.position.xyz - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.params.x + light.params.y * distance + light.params.z * (distance * distance));
    float theta = dot(lightDir, normalize(-light.direction.xyz));
    float epsilon = light.params.w - light.direction.w;
    float intensity = clamp((theta - light.direction.w) / epsilon, 0.0, 1.0);

    constexpr sampler s(mag_filter::linear, min_filter::linear);
    float3 ambient = light.ambient.xyz * diffuseMap.sample(s, texCoords).rgb;
    float3 diffuse = light.diffuse.xyz * diff * diffuseMap.sample(s, texCoords).rgb;
    float3 specular = light.specular.xyz * spec * specularMap.sample(s, texCoords).rgb;
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse + specular);
}

fragment float4 Mult_Object_fragment(VertexOut in [[stage_in]],
                                      constant UniformBlock& ubo [[buffer(8)]],
                                      texture2d<float> diffuseMap [[texture(0)]],
                                      texture2d<float> specularMap [[texture(1)]]) {
    float3 norm = normalize(in.normal.rgb);
    float3 viewDir = normalize(ubo.vec4Pool[0].xyz - in.FragPos.rgb);

    float3 direcLight = CalcDirLight(ubo.lights[0], norm, viewDir, diffuseMap, specularMap, in.TexCoords, ubo.floatPool[0]);
    float3 result = direcLight;
    for (int i = 0; i < 4; i++)
        result += CalcPointLight(ubo.lights[i + 1], norm, in.FragPos.rgb, viewDir, diffuseMap, specularMap, in.TexCoords, ubo.floatPool[0]);
    result += CalcSpotLight(ubo.lights[5], norm, in.FragPos.rgb, viewDir, diffuseMap, specularMap, in.TexCoords, ubo.floatPool[0]);

    return float4(result, 1.0);
}
