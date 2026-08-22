// 对应 res/GL/Advanced/Instance/Rock.fs：模型材质贴图。
// texture_diffuse1 由 Model 加载的 unit0 材质 → t1。
#include "../../_uniform_block.hlsli"

Texture2D gTextureDiffuse1 : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    return gTextureDiffuse1.Sample(gSamplerDefault, i.TexCoords);
}
