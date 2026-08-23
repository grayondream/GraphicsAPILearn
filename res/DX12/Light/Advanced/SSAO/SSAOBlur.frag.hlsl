// 对应 res/GL/Light/Advanced/SSAO/SSAOBlur.fs（4×4 盒式模糊，R32F 单通道输出）。
// ssaoInput→t1（Nearest+ClampToEdge→s4）。GLSL textureSize → GetDimensions(mip0)。
#include "../../../_uniform_block.hlsli"

Texture2D gSsaoInput : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

float PSMain(PSIn i) : SV_Target {
    uint width = 0;
    uint height = 0;
    gSsaoInput.GetDimensions(width, height);
    float2 texelSize = 1.0 / float2(width, height);
    float result = 0.0;
    [loop]
    for (int x = -2; x < 2; ++x) {
        [loop]
        for (int y = -2; y < 2; ++y) {
            float2 offset = float2(float(x), float(y)) * texelSize;
            result += gSsaoInput.Sample(gSamplerNearestClamp, i.TexCoords + offset).r;
        }
    }
    return result / (4.0 * 4.0);
}
