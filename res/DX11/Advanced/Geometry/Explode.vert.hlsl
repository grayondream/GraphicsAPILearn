// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0 编译，入口 VSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/Geometry/Explode.vs。
// layout 为 Sphere 的 RhiGeometry::Create(_object, false, true, true, Layout{0, 2})：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、normal@TEXCOORD2（binding2），
// 无 uv。VS 输出 clip 空间位置，GS 内做爆炸偏移。
#include "../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 inColor : TEXCOORD1;
    float4 aNormal : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
    float4 color : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.color = i.inColor;
    o.sv = mul(gProjection, mul(gView, mul(gModel, i.pos)));
    return o;
}
