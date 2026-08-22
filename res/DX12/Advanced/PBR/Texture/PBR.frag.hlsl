// 对应 res/GL/Advanced/PBR/Texture/PBR.fs：纹理版 Cook-Torrance PBR。
// 纹理寄存器约定 t<unit+1>：bindTexture(albedo,1)→t2、(roughness,2)→t3、(metallic,3)→t4、
// (ao,4)→t5、(normal,5)→t6；dFdx/dFdy→ddx/ddy；其余槽位与 Base 版一致。
#include "../../../_uniform_block.hlsli"

Texture2D gAlbedoMap : register(t2);
Texture2D gRoughnessMap : register(t3);
Texture2D gMetallicMap : register(t4);
Texture2D gAoMap : register(t5);
Texture2D gNormalMap : register(t6);

static const float PI = 3.14159265359;

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
    float3 Normal : TEXCOORD2;
};

float3 getNormalFromMap(float2 texCoords, float3 worldPos, float3 normal)
{
    float3 tangentNormal = gNormalMap.Sample(gSamplerDefault, texCoords).xyz * 2.0 - 1.0;

    float3 Q1 = ddx(worldPos);
    float3 Q2 = ddy(worldPos);
    float2 st1 = ddx(texCoords);
    float2 st2 = ddy(texCoords);

    float3 N = normalize(normal);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);

    return normalize(mul(TBN, tangentNormal));
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float4 PSMain(PSIn i) : SV_Target {
    float3 albedo = gAlbedoMap.Sample(gSamplerDefault, i.TexCoords).rgb;
    float metallic = gMetallicMap.Sample(gSamplerDefault, i.TexCoords).r;
    float roughness = gRoughnessMap.Sample(gSamplerDefault, i.TexCoords).r;
    float ao = gAoMap.Sample(gSamplerDefault, i.TexCoords).r;

    float3 N = getNormalFromMap(i.TexCoords, i.WorldPos, i.Normal);
    float3 V = normalize(gVec4Pool[1].xyz - i.WorldPos);   // camPos

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    float3 Lo = float3(0.0, 0.0, 0.0);
    [unroll]
    for (int k = 0; k < 4; ++k) {
        float3 L = normalize(gVec4Pool[13 + k].xyz - i.WorldPos);   // lightPositions[k]
        float3 H = normalize(V + L);
        float distance = length(gVec4Pool[13 + k].xyz - i.WorldPos);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = gVec4Pool[29 + k].xyz * attenuation;      // lightColors[k]

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        float3 F  = fresnelSchlick(saturate(dot(H, V)), F0);

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular = numerator / denominator;

        float3 kS = F;
        float3 kD = float3(1.0, 1.0, 1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * ao;

    float3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + 1.0.xxx);
    // gamma correct
    color = pow(color, (1.0 / 2.2).xxx);

    return float4(color, 1.0);
}
