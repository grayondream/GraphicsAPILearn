// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/Gamma/Source.fs：color = vec4Pool[3]（lightColor）。
#include "../../../_uniform_block.hlsli"

float4 PSMain() : SV_Target {
    return gVec4Pool[3];
}
