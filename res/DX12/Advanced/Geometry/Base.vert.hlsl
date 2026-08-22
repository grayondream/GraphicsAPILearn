// 对应 res/GL/Advanced/Geometry/Base.vs：点精灵房子（GS 输入）。
// layout 以 SimpleGemoteryApp 为准：binding0 交错 pos(Float2)@TEXCOORD0 +
// color(Float3)@TEXCOORD1（stride 20），Points 图元；z 恒 0。
struct VSIn {
    float2 aPos : TEXCOORD0;
    float3 aColor : TEXCOORD1;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 color : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.color = i.aColor;
    o.sv = float4(i.aPos.x, i.aPos.y, 0.0, 1.0);
    return o;
}
