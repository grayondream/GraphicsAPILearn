// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Ambination/Object.frag：FragColor = (0.2*lightColor) * objectColor。
// GLSL 死接口 outColor（声明未消费）未搬运。
#include "../../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
};

float4 PSMain(PSIn i) : SV_Target {
    float ambientStrength = 0.2;
    float4 ambient = ambientStrength * gVec4Pool[3];
    return ambient * gVec4Pool[4];
}
