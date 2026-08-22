// 对应 res/GL/Light/Advanced/NormalMap/NormalMap.fs（切线空间法线贴图）。
// 纹理寄存器约定 t<unit+1>：bindTexture(_brick,0)→t1、bindTexture(_brickNormal,1)→t2；
// 采样器取 RhiImage::Load2D 默认组合 LinearMipLinear+Repeat（s6）。enableNM→floatPool[22]。
#include "../../../_uniform_block.hlsli"

Texture2D gDiffuseMap : register(t1);
Texture2D gNormalMap : register(t2);

struct PSIn {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float2 TexCoords : TEXCOORD1;
    float3 TangentLightPos : TEXCOORD2;
    float3 TangentViewPos : TEXCOORD3;
    float3 TangentFragPos : TEXCOORD4;
    float3 aNormal : TEXCOORD5;
};

float4 PSMain(PSIn i) : SV_Target {
    // obtain normal from normal map in range [0,1]
    float3 normal = i.aNormal;   // gNormalMap.Sample(gSamplerDefault, i.TexCoords).rgb
    if (FPOOL(22) > 0.5) {
        normal = gNormalMap.Sample(gSamplerDefault, i.TexCoords).rgb;
    }
    // transform normal vector to range [-1,1]
    normal = normalize(normal * 2.0 - 1.0);   // this normal is in tangent space

    // get diffuse color
    float3 color = gDiffuseMap.Sample(gSamplerDefault, i.TexCoords).rgb;
    // ambient
    float3 ambient = 0.1 * color;
    // diffuse
    float3 lightDir = normalize(i.TangentLightPos - i.TangentFragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * color;
    // specular
    float3 viewDir = normalize(i.TangentViewPos - i.TangentFragPos);
    float3 reflectDir = reflect(-lightDir, normal);
    float3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    float3 specular = float3(0.2, 0.2, 0.2) * spec;
    return float4(ambient + diffuse + specular, 1.0);
}
