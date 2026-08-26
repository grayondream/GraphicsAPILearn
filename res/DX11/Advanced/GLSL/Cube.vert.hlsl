// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0 编译，入口 VSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/GLSL/Cube.vert：gl_VertexID→SV_VertexID（uint）。
// gl_PointSize 为 GL 专有点精灵尺寸（D3D11+ 移除 VS 点尺寸概念），无法直译，省略。
#include "../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 inColor : TEXCOORD1;
    float2 inTextureCoord : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
    float2 textureCoord : TEXCOORD0;
    float4 fragColor : TEXCOORD1;
};

VSOut VSMain(VSIn i, uint svVertexID : SV_VertexID) {
    VSOut o;
    o.sv = mul(gProjection, mul(gView, mul(gModel, i.pos)));
    o.textureCoord = i.inTextureCoord;
    if (FPOOL(43) > 0.5) {   // gl_PointSize 分支仅保留颜色部分
        o.fragColor = float4(0.0, 1.0, 1.0, 1.0);
    } else {
        o.fragColor = i.inColor;
    }

    if (FPOOL(42) > 0.5) {
        o.fragColor = float4(svVertexID % 2, svVertexID % 3, svVertexID % 4, 1.0);
    }
    return o;
}
