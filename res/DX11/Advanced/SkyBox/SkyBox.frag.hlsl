// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/SkyBox/SkyBox.frag：立方体天空盒采样。
// skybox 由 bindTexture(_skyBoxTexture, 1) 绑定 → t2（TextureCube）。
// 采样器用 gSamplerCubeLodAlign(s10)：对齐 GL/VK 参考的 cube 隐式 LOD 约定
// （见 _samplers.hlsli / DXPipeline.cpp 注释）。
#include "../../_uniform_block.hlsli"

TextureCube gSkybox : register(t2);

struct PSIn {
    float4 sv : SV_Position;
    float3 TexCoords : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    return gSkybox.Sample(gSamplerCubeLodAlign, i.TexCoords);
}
