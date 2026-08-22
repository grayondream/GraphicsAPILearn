// 对应 res/GL/Advanced/PBR/IBL_Specular/PBR.fs：完整 IBL（漫反射+镜面 Split-Sum）版 PBR。
// 纹理寄存器约定 t<unit+1>：bindTexture(irradianceMap,0)→t1、(prefilterMap,1)→t2、
// (brdfLUT,2)→t3；textureLod(prefilter)→SampleLevel；含 fresnelSchlickRoughness。
#include "../../../_uniform_block.hlsli"

TextureCube gIrradianceMap : register(t1);
TextureCube gPrefilterMap : register(t2);
Texture2D gBrdfLUT : register(t3);

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

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(1.0 - roughness.xxx, F0) - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float4 PSMain(PSIn i) : SV_Target {
    float3 albedo = gVec4Pool[10].xyz;   // albedo
    float metallic = FPOOL(7);           // metallic
    float roughness = FPOOL(8);          // roughness
    float ao = FPOOL(9);                 // ao

    float3 N = normalize(i.Normal);
    float3 V = normalize(gVec4Pool[1].xyz - i.WorldPos);   // camPos
    float3 R = reflect(-V, N);

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    // reflectance equation（直射光部分）
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

    // ambient lighting (we now use IBL as the ambient term)
    float3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    float3 kS = F;
    float3 kD = 1.0.xxx - kS;
    kD *= 1.0 - metallic;

    float3 irradiance = gIrradianceMap.Sample(gSamplerDefault, N).rgb;
    float3 diffuse = irradiance * albedo;

    // sample both the pre-filter map and the BRDF lut and combine them together as per
    // the Split-Sum approximation to get the specular part.
    const float MAX_REFLECTION_LOD = 4.0;
    float3 prefilteredColor = gPrefilterMap.SampleLevel(gSamplerDefault, R, roughness * MAX_REFLECTION_LOD).rgb;
    float2 brdf = gBrdfLUT.Sample(gSamplerDefault, float2(max(dot(N, V), 0.0), roughness)).rg;
    float3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    float3 ambient = (kD * diffuse + specular) * ao;

    float3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + 1.0.xxx);
    // gamma correct
    color = pow(color, (1.0 / 2.2).xxx);

    return float4(color, 1.0);
}
