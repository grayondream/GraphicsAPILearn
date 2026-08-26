// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T ps_5_0 编译，入口 PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/Instance/Saturn.fs：模型材质贴图（unit0 → t1）。
#include "../../_uniform_block.hlsli"

Texture2D gTextureDiffuse1 : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    return gTextureDiffuse1.Sample(gSamplerDefault, i.TexCoords);
}
