// DX11/SM5.0 起步文件（自 res/DX12/Base 同名拷贝，D3D NDC y-up 直绘无需翻转视口）。
// 顶点 layout 以 RectApp 的 RhiGeometry::Create(Rect) 为准：
// Float4 pos @TEXCOORD0 + Float4 color @TEXCOORD1（binding0 交错，stride 32）。
// 与 res/GL/Base/rect.vert 对应（GLSL location0/1 → TEXCOORD0/1）。
struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
};

struct VSOut {
    float4 sv : SV_Position;
    float4 col : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = i.pos;
    o.col = i.col;
    return o;
}
