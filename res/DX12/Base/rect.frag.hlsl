// 对应 res/GL/Base/rect.frag：直接输出插值顶点色。
// SV_Position 显式入参：新 dxc 对无 Position 单插量 PS 生成的硬件寄存器打包
// 与 VS 输出不一致，PSO 报 TEXCOORD linkage 错（全屏恒定灰、矩形不绘制）。
float4 PSMain(float4 sv : SV_Position, float4 col : TEXCOORD0) : SV_Target {
    return col;
}
