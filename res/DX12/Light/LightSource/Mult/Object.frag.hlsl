// 对应 res/GL/Light/LightSource/Mult/Object.frag（多光源：方向光 lights[0]+点光 lights[1..4]+聚光 lights[5]）。
// 纹理寄存器约定 t<unit+1>：bindTexture(_diffuseTex,0)→t1、bindTexture(_specularTex,1)→t2；
// 采样器取 RhiImage::Load2D 默认组合 LinearMipLinear+Repeat（s6）。
// GLSL 死接口 objOriginColor 未搬运；GLSL 块内 lights[6] 对应模板 gLights[256] 同序号槽。
#include "../../_samplers.hlsli"

Texture2D gDiffuseMap : register(t1);
Texture2D gSpecularMap : register(t2);

struct PSIn {
    float4 sv : SV_Position;
    float4 normal : TEXCOORD0;
    float4 fragPos : TEXCOORD1;
    float2 textureCoord : TEXCOORD2;
};

float3 CalcDirLight(ULight light, float3 normal, float3 viewDir, float2 texCoords) {
    float3 lightDir = normalize(-light.direction.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), FPOOL(0));
    float3 ambient = light.ambient.xyz * gDiffuseMap.Sample(gSamplerDefault, texCoords).rgb;
    float3 diffuse = light.diffuse.xyz * diff * gDiffuseMap.Sample(gSamplerDefault, texCoords).rgb;
    float3 specular = light.specular.xyz * spec * gSpecularMap.Sample(gSamplerDefault, texCoords).rgb;
    return (ambient + diffuse + specular);
}

float3 CalcPointLight(ULight light, float3 normal, float3 fragPos, float3 viewDir, float2 texCoords) {
    float3 lightDir = normalize(light.position.xyz - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), FPOOL(0));
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.params.x + light.params.y * distance + light.params.z * (distance * distance));
    float3 ambient = light.ambient.xyz * gDiffuseMap.Sample(gSamplerDefault, texCoords).rgb;
    float3 diffuse = light.diffuse.xyz * diff * gDiffuseMap.Sample(gSamplerDefault, texCoords).rgb;
    float3 specular = light.specular.xyz * spec * gSpecularMap.Sample(gSamplerDefault, texCoords).rgb;
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

float3 CalcSpotLight(ULight light, float3 normal, float3 fragPos, float3 viewDir, float2 texCoords) {
    float3 lightDir = normalize(light.position.xyz - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), FPOOL(0));
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.params.x + light.params.y * distance + light.params.z * (distance * distance));
    float theta = dot(lightDir, normalize(-light.direction.xyz));
    float epsilon = light.params.w - light.direction.w;
    float intensity = clamp((theta - light.direction.w) / epsilon, 0.0, 1.0);
    float3 ambient = light.ambient.xyz * gDiffuseMap.Sample(gSamplerDefault, texCoords).rgb;
    float3 diffuse = light.diffuse.xyz * diff * gDiffuseMap.Sample(gSamplerDefault, texCoords).rgb;
    float3 specular = light.specular.xyz * spec * gSpecularMap.Sample(gSamplerDefault, texCoords).rgb;
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse + specular);
}

float4 PSMain(PSIn i) : SV_Target {
    float3 norm = normalize(i.normal.rgb);
    float3 viewDir = normalize(gVec4Pool[0].xyz - i.fragPos.rgb);

    float3 direcLight = CalcDirLight(gLights[0], norm, viewDir, i.textureCoord);
    float3 result = direcLight;
    for (int k = 0; k < 4; k++)
        result += CalcPointLight(gLights[k + 1], norm, i.fragPos.rgb, viewDir, i.textureCoord);
    result += CalcSpotLight(gLights[5], norm, i.fragPos.rgb, viewDir, i.textureCoord);

    return float4(result, 1.0);
}
