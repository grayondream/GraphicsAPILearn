// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/Hdr/Hdr.fs（HDR tone mapping）。
// 纹理寄存器约定 t<unit+1>：bindTexture(_hdrRT->colorTexture2D(0),0)→t1；
// hdr→floatPool[13]、exposure→floatPool[4]。RT 附件采样用默认组合 s6。
#include "../../../_uniform_block.hlsli"

Texture2D gHdrBuffer : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    const float gamma = 2.2;
    float3 hdrColor = gHdrBuffer.Sample(gSamplerDefault, i.TexCoords).rgb;
    if (FPOOL(13) > 0.5) {   // hdr
        // reinhard
        // float3 result = hdrColor / (hdrColor + 1.0.xxx);
        // exposure
        float3 result = float3(1.0, 1.0, 1.0) - exp(-hdrColor * FPOOL(4));
        // also gamma correct while we're at it
        result = pow(result, 1.0 / gamma);
        return float4(result, 1.0);
    } else {
        float3 result = pow(hdrColor, 1.0 / gamma);
        return float4(result, 1.0);
    }
}
