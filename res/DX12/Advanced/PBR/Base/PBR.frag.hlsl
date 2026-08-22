// 对应 res/GL/Advanced/PBR/Base/PBR.fs：Cook-Torrance BRDF 直射光 PBR（无纹理，
// 材质参数走 pool：albedo=vec4Pool[10]、metallic=floatPool[7]、roughness=floatPool[8]、
// ao=floatPool[9]、camPos=vec4Pool[1]、lightPositions=vec4Pool[13+i]、lightColors=vec4Pool[29+i]）。
#include "../../../_uniform_block.hlsli"

static const float PI = 3.14159265359;

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
    float3 Normal : TEXCOORD2;
};

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
    float3 albedo = gVec4Pool[10].xyz;   // albedo
    float metallic = FPOOL(7);           // metallic
    float roughness = FPOOL(8);          // roughness
    float ao = FPOOL(9);                 // ao

    float3 N = normalize(i.Normal);
    float3 V = normalize(gVec4Pool[1].xyz - i.WorldPos);   // camPos

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    // reflectance equation
    float3 Lo = float3(0.0, 0.0, 0.0);
    [unroll]
    for (int k = 0; k < 4; ++k) {
        // calculate per-light radiance
        float3 L = normalize(gVec4Pool[13 + k].xyz - i.WorldPos);   // lightPositions[k]
        float3 H = normalize(V + L);
        float distance = length(gVec4Pool[13 + k].xyz - i.WorldPos);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = gVec4Pool[29 + k].xyz * attenuation;      // lightColors[k]

        // Cook-Torrance BRDF
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

    // ambient lighting（IBL 版本将替换为环境光照）
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * ao;

    float3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + 1.0.xxx);
    // gamma correct
    color = pow(color, (1.0 / 2.2).xxx);

    return float4(color, 1.0);
}
