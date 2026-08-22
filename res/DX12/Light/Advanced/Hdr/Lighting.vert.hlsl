// 顶点布局以 HdrApp::CreateCubeBuffer 为准：binding0 交错 pos@TEXCOORD0（Float3, offset0）+
// normal@TEXCOORD1（Float3, offset12）+ uv@TEXCOORD2（Float2, offset24），stride 32。
// 对应 res/GL/Light/Advanced/Hdr/Lighting.vs。GLSL 的 VS_OUT block 摊平为 TEXCOORD 插值器；
// GLSL 局部变量 normalMatrix 改名 nm 避免与 cbuffer 槽 gNormalMatrix 混淆。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float3 aPos : TEXCOORD0;
    float3 aNormal : TEXCOORD1;
    float2 aTexCoords : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoords : TEXCOORD2;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.FragPos = mul(gModel, float4(i.aPos, 1.0)).xyz;
    o.TexCoords = i.aTexCoords;

    float3 n = FPOOL(34) > 0.5 ? -i.aNormal : i.aNormal;   // inverse_normals

    float3x3 nm = transpose(inverse((float3x3)gModel));
    o.Normal = normalize(mul(nm, n));

    o.sv = mul(gProjection, mul(gView, mul(gModel, float4(i.aPos, 1.0))));
    return o;
}
