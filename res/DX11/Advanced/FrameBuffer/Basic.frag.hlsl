// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/FrameBuffer/Basic.frag：场景内平面直接贴图。
// textureSampler 由 bindTexture(…, 0) 绑定 → t1。
#include "../../_uniform_block.hlsli"

Texture2D gTextureSampler : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float2 textureCoord : TEXCOORD0;
    float4 fragColor : TEXCOORD1;
};

float4 PSMain(PSIn i) : SV_Target {
    return gTextureSampler.Sample(gSamplerDefault, i.textureCoord);
}
