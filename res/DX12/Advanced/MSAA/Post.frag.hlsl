// 对应 res/GL/Advanced/MSAA/Post.fs：灰度后处理。
// textureSampler 由 bindTexture(postFbo->colorTexture2D(0), 0) 绑定 → t1。
#include "../../../_uniform_block.hlsli"

Texture2D gTextureSampler : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float2 textureCoord : TEXCOORD0;
    float4 fragColor : TEXCOORD1;
};

float4 PSMain(PSIn i) : SV_Target {
    float3 col = gTextureSampler.Sample(gSamplerDefault, i.textureCoord).rgb;
    float grayscale = 0.2126 * col.r + 0.7152 * col.g + 0.0722 * col.b;
    return float4(grayscale.xxx, 1.0);
}
