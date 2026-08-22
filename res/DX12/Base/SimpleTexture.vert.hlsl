// 顶点 layout 以 SimpleTextureApp 的 RhiGeometry::Create(Rect, useUv) 为准：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32），uv@TEXCOORD2（binding1）。
// 对应 res/GL/Base/SimpleTexture.vert（GLSL location0/1/2）。无矩阵变换，pos 直传。
struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
    float4 col : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = i.pos;
    o.col = i.col;
    o.uv = i.uv;
    return o;
}
