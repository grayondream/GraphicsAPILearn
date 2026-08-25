// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Base/Cube.frag：color = texture(textureSampler, textureCoord)。
// 纹理寄存器约定 t<unit+1>：bindTexture(tex, 0) → t1；采样器 s6（Load2D 默认）。
#include "../_samplers.hlsli"

Texture2D gTex : register(t1);

float4 PSMain(float4 sv : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
    return gTex.Sample(gSamplerDefault, uv);
}
