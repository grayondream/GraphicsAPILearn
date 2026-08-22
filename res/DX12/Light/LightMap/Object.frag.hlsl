// 对应 res/GL/Light/LightMap/Object.frag（Blinn-Phong 双纹理光照）。
// 纹理寄存器约定 t<unit+1>：bindTexture(_diffuseTex,0)→t1、bindTexture(_specularTex,1)→t2；
// 采样器取 RhiImage::Load2D 默认组合 LinearMipLinear+Repeat（s6）。
// GLSL 死变量 viewValue / 死接口 objOriginColor 未搬运。
#include "../../_uniform_block.hlsli"

Texture2D gDiffuseMap : register(t1);
Texture2D gSpecularMap : register(t2);

struct PSIn {
    float4 sv : SV_Position;
    float4 normal : TEXCOORD0;
    float4 fragPos : TEXCOORD1;
    float2 textureCoord : TEXCOORD2;
};

float4 PSMain(PSIn i) : SV_Target {
    // ambient
    float4 ambient = gLights[0].ambient * float4(gDiffuseMap.Sample(gSamplerDefault, i.textureCoord).rgb, 1.0);

    // diffuse
    float4 norm = normalize(i.normal);
    float4 lightDir = normalize(gLights[0].position - i.fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    float4 diffuse = gLights[0].diffuse * diff * float4(gDiffuseMap.Sample(gSamplerDefault, i.textureCoord).rgb, 1.0);

    // specular
    float4 viewDir = normalize(gVec4Pool[0] - i.fragPos);
    float4 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), FPOOL(0));
    float4 specular = gLights[0].specular * spec * float4(gSpecularMap.Sample(gSamplerDefault, i.textureCoord).rgb, 1.0);

    // combination
    float4 result = ambient + diffuse + specular;
    return result;
}
