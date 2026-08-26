// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T ps_5_0 编译，入口 PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/PBR/IBL_IC/Background.fs：环境立方体背景 + tone map/gamma。
// environmentMap 由 bindTexture(m_envCubemap, 0) 绑定 → t1（TextureCube）。
#include "../../../_uniform_block.hlsli"

TextureCube gEnvironmentMap : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float3 WorldPos : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    float3 envColor = gEnvironmentMap.Sample(gSamplerDefault, i.WorldPos).rgb;

    // HDR tonemap and gamma correct
    envColor = envColor / (envColor + 1.0.xxx);
    envColor = pow(envColor, (1.0 / 2.2).xxx);

    return float4(envColor, 1.0);
}
