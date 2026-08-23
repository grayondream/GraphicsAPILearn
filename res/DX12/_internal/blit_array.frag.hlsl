// 内部 blit 像素着色器（数组变体）：cubemap mipgen 的源 SRV 视图为
// TEXTURE2DARRAY（单面单 mip，FirstArraySlice=f、ArraySize=1），按 Texture2D
// 采样属于视图类型不匹配的未定义行为——须以 Texture2DArray 声明采样，
// 数组轴取 0（视图内仅含一个切片）。
Texture2DArray gSrc : register(t0);
SamplerState gLinearClamp : register(s0);

float4 PSMain(float4 sv : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
    return gSrc.SampleLevel(gLinearClamp, float3(uv, 0.0f), 0.0f);
}
