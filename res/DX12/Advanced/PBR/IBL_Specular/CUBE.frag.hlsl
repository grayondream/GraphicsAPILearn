// 对应 res/GL/Advanced/PBR/IBL_IC/CUBE.fs：equirectangular 采样为 cube face 颜色。
// equirectangularMap 由 bindTexture(hdrEnvTexture, 0) 绑定 → t1；
// GLSL 双参 atan(z,x) → HLSL atan2。
#include "../../../_uniform_block.hlsli"

Texture2D gEquirectangularMap : register(t1);

static const float2 kInvAtan = float2(0.1591, 0.3183);

struct PSIn {
    float4 sv : SV_Position;
    float3 WorldPos : TEXCOORD0;
};

float2 SampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= kInvAtan;
    uv += 0.5;
    return uv;
}

float4 PSMain(PSIn i) : SV_Target {
    float2 uv = SampleSphericalMap(normalize(i.WorldPos));
    float3 color = gEquirectangularMap.Sample(gSamplerDefault, uv).rgb;
    return float4(color, 1.0);
}
