// 对应 res/GL/Light/Advanced/Shadow/ShadowMappingDepth.vs：深度写入 pass。
// layout 同 ShadowMapping（Cube 的 useUv=true,useNormal=true，默认 uv=2/normal=3）：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1、uv@TEXCOORD2（binding1）、normal@TEXCOORD3（binding2）。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
    float2 aTexCoord : TEXCOORD2;
    float4 aNormal : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = mul(gExtraMat4[0], mul(gModel, i.pos));   // extraMat4[0]=lightSpaceMatrix
    return o;
}
