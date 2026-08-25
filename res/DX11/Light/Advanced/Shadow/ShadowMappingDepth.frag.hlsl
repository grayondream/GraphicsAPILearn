// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/Shadow/ShadowMappingDepth.fs：深度写入 pass 无颜色输出
// （渲染目标仅有 Depth32F 附件），PSMain 无 SV_Target 返回。
#include "../../../_uniform_block.hlsli"

void PSMain() {
}
