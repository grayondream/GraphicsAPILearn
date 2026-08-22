// 对应 res/GL/Light/Advanced/ShadowMap/ShadowMapping.vs：深度写入 pass。
// 管线 layout 为 Sphere 的 RhiGeometry::Create(_object, false, false, true)：binding0
// 交错 pos@TEXCOORD0 + color@TEXCOORD1；绘制 plane 时 IA 另提供 uv@2/normal@3，
// shader 未声明即不消费（D3D12 允许多余 IA 语义）。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
};

struct VSOut {
    float4 sv : SV_Position;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = mul(gExtraMat4[0], mul(gModel, i.pos));   // extraMat4[0]=lightSpaceMatrix
    return o;
}
