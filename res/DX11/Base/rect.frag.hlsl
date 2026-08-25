// DX11/SM5.0 起步文件，对应 res/GL/Base/rect.frag：直接输出插值顶点色。
// SV_Position 显式入参：保持与 DX12 版一致——无 Position 单插量 PS 的硬件寄存器
// 打包与 VS 输出不一致时会报 TEXCOORD linkage 错（全屏恒定灰、矩形不绘制）。
float4 PSMain(float4 sv : SV_Position, float4 col : TEXCOORD0) : SV_Target {
    return col;
}
