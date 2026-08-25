// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/ShadowMap/Depth.vs：debug quad，pos 直接作 clip 坐标。
// layout 为 Rect 的 RhiGeometry::Create(_object, true, false, true)：binding0 交错
// pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、uv@TEXCOORD2（binding1）。
// GLSL 死插值器 FragPos/Normal（fs 未消费，rect 几何亦无 normal 数据）未搬运。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
    float2 textureCoord : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.TexCoords = i.textureCoord;
    o.sv = i.pos;
    return o;
}
