// 对应 res/GL/Light/Advanced/PointLightShadow/ShadowMappingDepth.vs。
// layout 为 Cube 的 RhiGeometry::Create(_object, true, true, true)（默认 uv=2/normal=3）：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1、uv@TEXCOORD2（binding1）、normal@TEXCOORD3（binding2）。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 aPos : TEXCOORD0;
    float4 aColor : TEXCOORD1;
    float2 aTexCoord : TEXCOORD2;
    float4 aNormal : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
    float4 FragPos : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    float4 worldPos = mul(gModel, i.aPos);
    o.FragPos = worldPos;
    // 当前面的 shadow matrix（固定槽 extraMat4[1] = shadowMatrices[0]，逐面 update）
    o.sv = mul(gExtraMat4[1], worldPos);
    return o;
}
