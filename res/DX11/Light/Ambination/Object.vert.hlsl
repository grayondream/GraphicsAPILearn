// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 物体管线以 SimpleLightAmbination 的 RhiGeometry::Create(shape, normal+index,
// {.normalLocation=2}) 为准：binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、
// normal@TEXCOORD2（binding2，无 uv）。对应 res/GL/Light/Ambination/Object.vert。
// GLSL 死接口 outColor/normal 输出中 frag 仅消费 normal，outColor 未搬运。
#include "../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
    float4 normal : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = mul(gProjection, mul(gView, mul(gModel, i.pos)));
    return o;
}
