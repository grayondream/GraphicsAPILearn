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
