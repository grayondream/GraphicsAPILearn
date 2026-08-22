// 对应 res/GL/Advanced/PBR/Base/PBR.vs。
// layout 为 Sphere 的 RhiGeometry::Create(_object, true, true, true)（默认 uv=2/normal=3）：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1、uv@TEXCOORD2（binding1）、normal@TEXCOORD3（binding2）。
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
