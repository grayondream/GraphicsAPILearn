// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 内部 mip 降采样专用像素着色器（数组变体）：cubemap mipgen 的源 SRV 视图为
// TEXTURE2DARRAY（单面单 mip，FirstArraySlice=f、ArraySize=1），须以
// Texture2DArray 声明采样（同 blit_array.frag）。Gather 的 layer 轴按最近邻
// 取整，视图仅含一个切片故恒取 z=0；层内按 uv 角点做等权盒平均（对齐
// vkCmdBlitImage linear 的 2:1 盒式语义）。
Texture2DArray gSrc : register(t0);
SamplerState gLinearClamp : register(s0);

float4 PSMain(float4 sv : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
    const float3 loc = float3(uv, 0.0f);
    float4 r = gSrc.GatherRed(gLinearClamp, loc);
    float4 g = gSrc.GatherGreen(gLinearClamp, loc);
    float4 b = gSrc.GatherBlue(gLinearClamp, loc);
    float4 a = gSrc.GatherAlpha(gLinearClamp, loc);
    const float4 quarter = float4(0.25f, 0.25f, 0.25f, 0.25f);
    return float4(dot(r, quarter), dot(g, quarter), dot(b, quarter), dot(a, quarter));
}
