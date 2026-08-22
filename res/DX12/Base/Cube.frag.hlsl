// 对应 res/GL/Base/Cube.frag：color = texture(textureSampler, textureCoord)。
// 纹理寄存器约定 t<unit+1>：bindTexture(tex, 0) → t1；采样器 s6（Load2D 默认）。
#include "../_samplers.hlsli"

Texture2D gTex : register(t1);

float4 PSMain(float4 sv : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
    return gTex.Sample(gSamplerDefault, uv);
}
