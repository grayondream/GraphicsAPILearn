// 静态采样器别名表（root signature 静态采样器槽位 = f*3+w，见 DXPipeline::StaticSamplers）：
//   filter: Linear=0 / Nearest=1 / LinearMipLinear=2
//   wrap:   Repeat=0 / ClampToEdge=1 / ClampToBorder=2（borderColor 统一 OPAQUE_WHITE）
// D3D12 静态采样器无法在 shader 内动态索引 → 各 shader 按纹理实际的 filter/wrap
// 在此取对应别名（编译期固定寄存器，绑定随根签名一次生效，无需每 draw 动作）。
// gSamplerDefault(s6) = LinearMipLinear+Repeat，即 RhiImage::Load2D 的默认组合。
#ifndef DX_SAMPLERS_HLSLI
#define DX_SAMPLERS_HLSLI

SamplerState gSamplerLinearRepeat  : register(s0); // f=0 w=0
SamplerState gSamplerLinearClamp   : register(s1); // f=0 w=1
SamplerState gSamplerLinearBorder  : register(s2); // f=0 w=2
SamplerState gSamplerNearestRepeat : register(s3); // f=1 w=0
SamplerState gSamplerNearestClamp  : register(s4); // f=1 w=1
SamplerState gSamplerNearestBorder : register(s5); // f=1 w=2
SamplerState gSamplerDefault       : register(s6); // f=2 w=0（Load2D 默认）
SamplerState gSamplerMipLinearClamp  : register(s7); // f=2 w=1
SamplerState gSamplerMipLinearBorder : register(s8); // f=2 w=2
// s9 不在 f*3+w 组合空间内：shadow map 专用硬件比较采样器（DXPipeline.cpp StaticSamplers
// 同步提供静态条目），Nearest+LESS_EQUAL+ClampToBorder(OPAQUE_WHITE)。
// SampleCmp 返回 ref <= stored ? 1 : 0（lit=1）；GLSL 手动比较的 shadow 取 1-lit。
SamplerComparisonState gShadowCompare : register(s9);
// s10：cubemap LOD 对齐（SkyBox 组专用）。GL/VK 参考实现对立方体纹理的隐式 LOD
// 约定比 D3D12 高约 +0.28（跨 API cube-LOD 公式差异，实测参考输出恒为纯 mip1），
// 以 MipLODBias 吸收，使 DX12 与参考逐像素对齐；不影响其余样例的 s6。
SamplerState gSamplerCubeLodAlign : register(s10); // f=2 w=0 + MipLODBias 0.28
// s11：2D 纹理 LOD 对齐（SkyBox 组 cube 物体的 dog.jpg 用）。同 s10 的约定差，
// 2D 路径实测偏差更大（约 +1.10），单独设槽避免与 cubemap 对齐量互相牵制。
SamplerState gSamplerTex2DLodAlign : register(s11); // f=2 w=0 + MipLODBias 1.10

#endif
