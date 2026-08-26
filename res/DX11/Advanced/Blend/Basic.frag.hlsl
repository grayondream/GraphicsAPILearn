// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/Blend/Basic.frag：texColor（vec4Pool[6]）alpha<0.1 走贴图否则纯色。
// textureSampler 由 bindTexture(…, 0) 绑定 → t1。
#include "../../_uniform_block.hlsli"

Texture2D gTextureSampler : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
    float4 fragColor : TEXCOORD1;
};

float4 PSMain(PSIn i) : SV_Target {
    if (gVec4Pool[6].w < 0.1) {
        return gTextureSampler.Sample(gSamplerDefault, i.TexCoords);
    } else {
        return gVec4Pool[6];
    }
}
