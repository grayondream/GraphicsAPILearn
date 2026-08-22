// 对应 res/GL/Light/Advanced/Shadow/DebugQuand.vs：全屏 quad，pos 直接作 clip 坐标。
// layout 以 ShadowApp::createQuadBuffer 为准：binding0 交错 pos(Float3)@TEXCOORD0 +
// uv(Float2)@TEXCOORD1（stride 20），4 顶点 TriangleStrip。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float3 aPos : TEXCOORD0;
    float2 aTexCoords : TEXCOORD1;
};

struct VSOut {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.TexCoords = i.aTexCoords;
    o.sv = float4(i.aPos, 1.0);
    return o;
}
