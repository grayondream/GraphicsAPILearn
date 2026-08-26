// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 内部 mip 降采样专用像素着色器：dst 像素中心 UV 恰是 4 个 src 纹素中心构成的
// 正方形共享角点，在该点 Gather 返回的 4 值即等权盒平均——对齐 VK
// vkCmdBlitImage linear 对 2:1 缩放的盒式语义（单点线性 Sample 会混叠出散点差异）。
// 仅 mipgen 使用；RT↔RT 颜色拷贝继续用 blit.frag 的恒等线性映射。
Texture2D gSrc : register(t0);
SamplerState gLinearClamp : register(s0);

float4 PSMain(float4 sv : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
    float4 r = gSrc.GatherRed(gLinearClamp, uv);
    float4 g = gSrc.GatherGreen(gLinearClamp, uv);
    float4 b = gSrc.GatherBlue(gLinearClamp, uv);
    float4 a = gSrc.GatherAlpha(gLinearClamp, uv);
    const float4 quarter = float4(0.25f, 0.25f, 0.25f, 0.25f);
    return float4(dot(r, quarter), dot(g, quarter), dot(b, quarter), dot(a, quarter));
}
