// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/Shadow/DebugQuand.fs：debug quad 显示深度值（正交）。
// depthMap 由 bindTexture(fbo->depthTexture2D(), 0) 绑定 → t1；
// LinearizeDepth 死代码（源码已注释）未搬运。
#include "../../../_uniform_block.hlsli"

Texture2D gDepthMap : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    float depthValue = gDepthMap.Sample(gSamplerDefault, i.TexCoords).r;
    return float4(depthValue.xxx, 1.0);   // orthographic
}
