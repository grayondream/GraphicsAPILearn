#include "../_uniform_block.hlsli"

// DX11/SM5.0 起步文件（自 res/DX12/Base 同名拷贝，语义约定一致）。
// 顶点 layout 以 TriangleApp 的 RhiGeometry::Create(Triangle) 为准：
// Float4 pos @semantic0 + Float4 color @semantic1（stride 32）
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
