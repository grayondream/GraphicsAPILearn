// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/Defer/GBuffer.fs（GBuffer 写入，MRT 三输出）。
// 物体管线以 DeferApp 的 RhiGeometry::Create(cube, uv+normal+index, 默认布局) 为准：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、uv@TEXCOORD2（binding1）、
// normal@TEXCOORD3（binding2）。GBuffer 三附件 Nearest+ClampToEdge。
// 纹理寄存器约定 t<unit+1>：wood/brick 均 bindTexture unit0 → t1（s6 默认组合）。
#include "../../../_uniform_block.hlsli"

Texture2D gDiffuseTexture : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float2 TexCoords : TEXCOORD1;
    float3 Normal : TEXCOORD2;
};

struct PSOut {
    float3 gPosition : SV_Target0;
    float3 gNormal : SV_Target1;
    float4 gAlbedoSpec : SV_Target2;
};

PSOut PSMain(PSIn i) {
    PSOut o;
    o.gPosition = i.FragPos;
    o.gNormal = normalize(i.Normal);
    float4 tex = gDiffuseTexture.Sample(gSamplerDefault, i.TexCoords);
    o.gAlbedoSpec = float4(tex.rgb, tex.r);
    return o;
}
