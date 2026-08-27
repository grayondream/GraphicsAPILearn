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
    float3 WorldPos;
};

vertex VertexOut IBL_Specular_Prefilter_vertex(VertexIn in [[stage_in]],
                                               constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.WorldPos = in.pos.xyz;
    out.position = ubo.projection * ubo.view * float4(out.WorldPos, 1.0);
    return out;
}

fragment float4 IBL_Specular_Prefilter_fragment(VertexOut in [[stage_in]],
                                                constant UniformBlock& ubo [[buffer(0)]],
                                                texturecube<float> environmentMap [[texture(0)]],
                                                sampler smp [[sampler(0)]]) {
    const float PI = 3.14159265359;
    
    auto RadicalInverse_VdC = [](uint bits) -> float {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return float(bits) * 2.3283064365386963e-10;
    };
    
    auto Hammersley = [&RadicalInverse_VdC](uint i, uint N) -> float2 {
        return float2(float(i) / float(N), RadicalInverse_VdC(i));
    };
    
    auto ImportanceSampleGGX = [&PI](float2 Xi, float3 N, float roughness) -> float3 {
        float a = roughness * roughness;
        float phi = 2.0 * PI * Xi.x;
        float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        
        float3 H;
        H.x = cos(phi) * sinTheta;
        H.y = sin(phi) * sinTheta;
        H.z = cosTheta;
        
        float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
        float3 tangent = normalize(cross(up, N));
        float3 bitangent = cross(N, tangent);
        
        float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
        return normalize(sampleVec);
    };
    
    auto DistributionGGX = [&PI](float3 N, float3 H, float roughness) -> float {
        float a = roughness * roughness;
        float a2 = a * a;
        float NdotH = max(dot(N, H), 0.0);
        float NdotH2 = NdotH * NdotH;
        float nom = a2;
        float denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = PI * denom * denom;
        return nom / denom;
    };
    
    float3 N = normalize(in.WorldPos);
    float3 R = N;
    float3 V = R;
    
    const uint SAMPLE_COUNT = 1024u;
    float3 prefilteredColor = float3(0.0);
    float totalWeight = 0.0;
    
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, ubo.floatPool[8]);
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        
        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
            float D = DistributionGGX(N, H, ubo.floatPool[8]);
            float NdotH2 = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf = D * NdotH2 / (4.0 * HdotV) + 0.0001;
            
            float resolution = 512.0;
            float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
            
            float mipLevel = ubo.floatPool[8] == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);
            
            prefilteredColor += environmentMap.sample(smp, L, level(mipLevel)).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    
    prefilteredColor = prefilteredColor / totalWeight;
    
    return float4(prefilteredColor, 1.0);
}