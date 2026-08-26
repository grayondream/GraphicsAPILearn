// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/UniformBuffer/Cube.frag：cubeColor（vec4Pool[5]）调制。
#include "../../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
    float4 fragColor : TEXCOORD0;
    float2 textureCoord : TEXCOORD1;
};

float4 PSMain(PSIn i) : SV_Target {
    return gVec4Pool[5] * i.fragColor;
}
