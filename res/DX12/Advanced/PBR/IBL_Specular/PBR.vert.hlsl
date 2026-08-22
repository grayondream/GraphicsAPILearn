// 对应 res/GL/Advanced/PBR/IBL_IC/PBR.vs（与 Base 版一致）。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
    float2 aTexCoords : TEXCOORD2;
    float4 normal : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
    float3 Normal : TEXCOORD2;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.TexCoords = i.aTexCoords;
    o.WorldPos = mul(gModel, i.pos).xyz;
    o.Normal = mul((float3x3)gNormalMatrix, i.normal.xyz);
    o.sv = mul(gProjection, mul(gView, float4(o.WorldPos, 1.0)));
    return o;
}
