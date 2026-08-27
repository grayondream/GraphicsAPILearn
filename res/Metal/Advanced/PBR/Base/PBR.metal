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
    float2 aTexCoords [[attribute(2)]];
    float4 normal [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 TexCoords;
    float3 WorldPos;
    float3 Normal;
};

vertex VertexOut PBR_Base_vertex(VertexIn in [[stage_in]],
                                 constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.TexCoords = in.aTexCoords;
    out.WorldPos = (ubo.model * in.pos).xyz;
    out.Normal = (ubo.normalMatrix * in.normal).xyz;
    out.position = ubo.projection * ubo.view * float4(out.WorldPos, 1.0);
    return out;
}

fragment float4 PBR_Base_fragment(VertexOut in [[stage_in]],
                                  constant UniformBlock& ubo [[buffer(0)]]) {
    const float PI = 3.14159265359;
    
    float3 albedo = ubo.vec4Pool[10].xyz;
    float metallic = ubo.floatPool[7];
    float roughness = ubo.floatPool[8];
    float ao = ubo.floatPool[9];
    
    float3 N = normalize(in.Normal);
    float3 V = normalize(ubo.vec4Pool[1].xyz - in.WorldPos);
    
    float3 F0 = float3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    float3 Lo = float3(0.0);
    for(int i = 0; i < 4; ++i)
    {
        float3 L = normalize(ubo.vec4Pool[13 + i].xyz - in.WorldPos);
        float3 H = normalize(V + L);
        float distance = length(ubo.vec4Pool[13 + i].xyz - in.WorldPos);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = ubo.vec4Pool[29 + i].xyz * attenuation;
        
        float a = roughness * roughness;
        float a2 = a * a;
        float NdotH = max(dot(N, H), 0.0);
        float NdotH2 = NdotH * NdotH;
        float nom = a2;
        float denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = PI * denom * denom;
        float NDF = nom / denom;
        
        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        float r = (roughness + 1.0);
        float k = (r * r) / 8.0;
        float ggx2 = NdotV / (NdotV * (1.0 - k) + k);
        float ggx1 = NdotL / (NdotL * (1.0 - k) + k);
        float G = ggx1 * ggx2;
        
        float3 F = F0 + (1.0 - F0) * pow(clamp(1.0 - dot(H, V), 0.0, 1.0), 5.0);
        
        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular = numerator / denominator;
        
        float3 kS = F;
        float3 kD = float3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    float3 ambient = float3(0.03) * albedo * ao;
    float3 color = ambient + Lo;
    
    color = color / (color + float3(1.0));
    color = pow(color, float3(1.0/2.2));
    
    return float4(color, 1.0);
}