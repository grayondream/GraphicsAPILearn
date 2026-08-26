// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T ps_5_0 编译，入口 PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/GLSL/Cube.frag：gl_FragCoord→SV_Position.xy、gl_FrontFacing→
// SV_IsFrontFace；textureSampler 死纹理（主路径未采样，源码注释行）未搬运。
#include "../../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
    float2 textureCoord : TEXCOORD0;
    float4 fragColor : TEXCOORD1;
};

float4 PSMain(PSIn i, bool svFrontFacing : SV_IsFrontFace) : SV_Target {
    float4 color = i.fragColor;
    if (FPOOL(38) > 0.5) {
        if (i.sv.x < 400)
            color = float4(1.0, 0.0, 0.0, 1.0);
        else
            color = float4(0.0, 1.0, 0.0, 1.0);
    }

    if (FPOOL(44) > 0.5) {
        if (svFrontFacing) {
            color = float4(0.0, 0.0, 1.0, 1.0);
        } else {
            color = float4(1.0, 1.0, 0.0, 1.0);
        }
    }
    return color;
}
