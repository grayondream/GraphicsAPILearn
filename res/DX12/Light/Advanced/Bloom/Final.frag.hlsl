// 对应 res/GL/Light/Advanced/Bloom/Final.fs（bloom 叠加 + exposure tone mapping）。
// 纹理寄存器约定 t<unit+1>：bindTexture(m_hdrFBO color[0],0)→t1、bindTexture(pingpong[1],1)→t2；
// bloom→floatPool[11]、exposure→floatPool[4]。RT 附件采样用默认组合 s6。
#include "../../../_uniform_block.hlsli"

Texture2D gScene : register(t1);
Texture2D gBloomBlur : register(t2);

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    const float gamma = 2.2;
    float3 hdrColor = gScene.Sample(gSamplerDefault, i.TexCoords).rgb;
    float3 bloomColor = gBloomBlur.Sample(gSamplerDefault, i.TexCoords).rgb;
    if (FPOOL(11) > 0.5)   // bloom
        hdrColor += bloomColor;   // additive blending
    // tone mapping
    float3 result = float3(1.0, 1.0, 1.0) - exp(-hdrColor * FPOOL(4));   // exposure
    // also gamma correct while we're at it
    result = pow(result, 1.0 / gamma);
    return float4(result, 1.0);
}
