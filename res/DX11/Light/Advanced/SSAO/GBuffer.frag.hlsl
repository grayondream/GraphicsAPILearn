// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/SSAO/GBuffer.fs（GBuffer 写入，MRT 三输出，纯色 albedo）。
// 布局同 SSAO/GBuffer.vert.hlsl；三附件 RGBA16F/RGBA16F/RGBA8，Nearest+ClampToEdge。
#include "../../../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float2 TexCoords : TEXCOORD1;
    float3 Normal : TEXCOORD2;
};

struct PSOut {
    float3 gPosition : SV_Target0;
    float3 gNormal : SV_Target1;
    float3 gAlbedo : SV_Target2;
};

PSOut PSMain(PSIn i) {
    PSOut o;
    o.gPosition = i.FragPos;
    o.gNormal = normalize(i.Normal);
    o.gAlbedo = float3(0.95, 0.95, 0.95);
    return o;
}
