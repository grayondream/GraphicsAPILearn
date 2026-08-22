// 对应 res/GL/Advanced/SkyBox/SkyBox.frag：立方体天空盒采样。
// skybox 由 bindTexture(_skyBoxTexture, 1) 绑定 → t2（TextureCube）。
#include "../../../_uniform_block.hlsli"

TextureCube gSkybox : register(t2);

struct PSIn {
    float4 sv : SV_Position;
    float3 TexCoords : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    return gSkybox.Sample(gSamplerDefault, i.TexCoords);
}
