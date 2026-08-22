// 对应 res/GL/Advanced/PBR/IBL_Irradiance/Irradiance.vs。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
    float2 aTexCoords : TEXCOORD2;
    float4 normal : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 WorldPos : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.WorldPos = i.pos.xyz;
    o.sv = mul(gProjection, mul(gView, float4(o.WorldPos, 1.0)));
    return o;
}
