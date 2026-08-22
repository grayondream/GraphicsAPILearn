// 对应 res/GL/Light/Advanced/Bloom/Blur.fs（高斯模糊 pingpong）。
// 纹理寄存器约定 t<unit+1>：bindTexture(hdr/pingpong 颜色附件,0)→t1；
// horizontal→floatPool[12]。textureSize(image,0) 对应 GetDimensions（mip0 尺寸）。
#include "../../../_uniform_block.hlsli"

Texture2D gImage : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

static const float weight[5] = { 0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162 };

float4 PSMain(PSIn i) : SV_Target {
    float texW, texH;
    gImage.GetDimensions(texW, texH);
    float2 tex_offset = 1.0 / float2(texW, texH);   // gets size of single texel
    float3 result = gImage.Sample(gSamplerDefault, i.TexCoords).rgb * weight[0];
    if (FPOOL(12) > 0.5) {   // horizontal
        for (int k = 1; k < 5; ++k) {
            result += gImage.Sample(gSamplerDefault, i.TexCoords + float2(tex_offset.x * k, 0.0)).rgb * weight[k];
            result += gImage.Sample(gSamplerDefault, i.TexCoords - float2(tex_offset.x * k, 0.0)).rgb * weight[k];
        }
    } else {
        for (int k = 1; k < 5; ++k) {
            result += gImage.Sample(gSamplerDefault, i.TexCoords + float2(0.0, tex_offset.y * k)).rgb * weight[k];
            result += gImage.Sample(gSamplerDefault, i.TexCoords - float2(0.0, tex_offset.y * k)).rgb * weight[k];
        }
    }
    return float4(result, 1.0);
}
