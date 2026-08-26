// 采样器别名表（SM5.0/DX11：SamplerState 寄存器编号 = f*3+w，见 DX11Renderer 采样器
// 绑定约定，Task 5 纹理接入时按纹理实际 filter/wrap 取对应槽位）：
//   filter: Linear=0 / Nearest=1 / LinearMipLinear=2
//   wrap:   Repeat=0 / ClampToEdge=1 / ClampToBorder=2（borderColor 统一白）
// 各 shader 按需在此取别名（编译期固定寄存器 s#，运行期经 PSSetSamplers 槽位绑定）。
// gSamplerDefault(s6) = LinearMipLinear+Repeat 静态初值；运行期 Renderer 在每次
// bindTexture 时按该纹理的 (minFilter, wrapS, borderColor) 精确换装本寄存器
// （GL"采样器状态跟纹理走"语义；Bloom 地板 Clamp 纹理曾被静态 Repeat 平铺的教训）。
// 故 shader 侧常规采样统一写 gSamplerDefault 即可，勿按纹理 wrap 挑别名。
#ifndef DX11_SAMPLERS_HLSLI
#define DX11_SAMPLERS_HLSLI

SamplerState gSamplerLinearRepeat  : register(s0); // f=0 w=0
SamplerState gSamplerLinearClamp   : register(s1); // f=0 w=1
SamplerState gSamplerLinearBorder  : register(s2); // f=0 w=2
SamplerState gSamplerNearestRepeat : register(s3); // f=1 w=0
SamplerState gSamplerNearestClamp  : register(s4); // f=1 w=1
SamplerState gSamplerNearestBorder : register(s5); // f=1 w=2
SamplerState gSamplerDefault       : register(s6); // f=2 w=0（Load2D 默认）
SamplerState gSamplerMipLinearClamp  : register(s7); // f=2 w=1
SamplerState gSamplerMipLinearBorder : register(s8); // f=2 w=2
// s9 不在 f*3+w 组合空间内：shadow map 专用硬件比较采样器（Task 阴影组接入时提供，
// Nearest+LESS_EQUAL+ClampToBorder(白)）。
// SampleCmp 返回 ref <= stored ? 1 : 0（lit=1）；GLSL 手动比较的 shadow 取 1-lit。
SamplerComparisonState gShadowCompare : register(s9);
// s10/s11：SkyBox 组 LOD 对齐采样器（对照 res/DX12 同名别名；寄存器编号一致）。
// bias 数值由 Renderer 侧预建采样器给出——DX11 初值 0（brief 指定预留），若与 GL
// 参照 diff 超标再校准并记录数值（对照 DX12 NVIDIA 实测 0.28/0.85）。
SamplerState gSamplerCubeLodAlign  : register(s10); // f=2 w=0 + MipLODBias（Renderer 定）
SamplerState gSamplerTex2DLodAlign : register(s11); // f=2 w=0 + MipLODBias（Renderer 定）

#endif
