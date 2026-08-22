// 对应 res/GL/Base/SimpleTexture.frag：color = texture * fragColor。
// 纹理寄存器约定 t<unit+1>（槽 0 预留 ImGui）：bindTexture(tex, 0) → t1；
// 采样器取 RhiImage::Load2D 默认组合 LinearMipLinear+Repeat（s6）。
#include "../_samplers.hlsli"

Texture2D gTex : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float4 col : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

float4 PSMain(PSIn i) : SV_Target {
    return gTex.Sample(gSamplerDefault, i.uv) * i.col;
}
