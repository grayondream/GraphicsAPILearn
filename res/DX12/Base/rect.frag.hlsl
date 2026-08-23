// 对应 res/GL/Base/rect.frag：直接输出插值顶点色。
// 入参带 SV_Position（像素坐标，未使用）：新 dxc 对"无 Position 输入、单插量"
// 的 PS 会生成与 VS 不一致的硬件寄存器打包，PSO 验证报 TEXCOORD linkage 错误；
// 全项目其余可链接的 PS 均带该入参，此处对齐约定。
float4 PSMain(float4 col : TEXCOORD0, float4 sv : SV_Position) : SV_Target {
    return col;
}
