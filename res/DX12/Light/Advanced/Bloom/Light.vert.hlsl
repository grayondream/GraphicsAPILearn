// 物体管线以 BloomApp 的 RhiGeometry::Create(cube, uv+normal+index, 默认布局) 为准：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、uv@TEXCOORD2（binding1）、
// normal@TEXCOORD3（binding2）。对应 res/GL/Light/Advanced/Bloom/Light.vs。
// GLSL 的 VS_OUT block 摊平为 TEXCOORD 插值器。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 aPos : TEXCOORD0;
    float4 aColor : TEXCOORD1;
    float2 aTexCoord : TEXCOORD2;
    float4 aNormal : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoords : TEXCOORD2;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.FragPos = mul(gModel, i.aPos).xyz;
    o.TexCoords = i.aTexCoord;

    float3x3 nm = transpose(Mat3Inverse((float3x3)gModel));
    o.Normal = normalize(mul(nm, i.aNormal.xyz));

    o.sv = mul(gProjection, mul(gView, mul(gModel, i.aPos)));
    return o;
}
