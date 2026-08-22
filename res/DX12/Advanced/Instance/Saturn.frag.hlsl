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
