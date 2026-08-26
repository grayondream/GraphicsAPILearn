// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/Defer/LightBox.fs（光源盒纯色输出）。
// lightColor→vec4Pool[3]。
#include "../../../_uniform_block.hlsli"

float4 PSMain(float4 sv : SV_Position) : SV_Target {
    return float4(gVec4Pool[3].rgb, 1.0);   // lightColor
}
