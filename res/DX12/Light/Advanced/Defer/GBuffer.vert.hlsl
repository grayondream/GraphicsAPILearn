// 对应 res/GL/Light/Advanced/Defer/GBuffer.vs（GBuffer 顶点写入）。
// 物体管线以 DeferApp 的 RhiGeometry::Create(cube, uv+normal+index, 默认布局) 为准：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、uv@TEXCOORD2（binding1）、
// normal@TEXCOORD3（binding2）。对应 res/GL/Light/Advanced/Defer/GBuffer.vs。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 aPos : TEXCOORD0;
    float4 aColor : TEXCOORD1;   // 未使用，仅为对齐输入布局占位
    float2 aTexCoord : TEXCOORD2;
    float4 aNormal : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float2 TexCoords : TEXCOORD1;
    float3 Normal : TEXCOORD2;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    float4 worldPos = mul(gModel, i.aPos);
    o.FragPos = worldPos.xyz;
    o.TexCoords = i.aTexCoord;

    float3x3 nm = transpose(Mat3Inverse((float3x3)gModel));
    o.Normal = mul(nm, i.aNormal.xyz);

    o.sv = mul(gProjection, mul(gView, worldPos));
    return o;
}
