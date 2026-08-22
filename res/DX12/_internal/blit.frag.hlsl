// 内部 blit 像素着色器：线性采样源纹理（mip 降采样 / RT↔RT 颜色拷贝共用）。
// cbuffer 精简为无：变换全在视口/UV 恒等映射内完成。
Texture2D gSrc : register(t0);
SamplerState gLinearClamp : register(s0);

float4 PSMain(float4 sv : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
    return gSrc.Sample(gLinearClamp, uv);
}
