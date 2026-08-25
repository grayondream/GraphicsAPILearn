// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/Shadow/ShadowMapping.vs。
// layout 为 Cube 的 RhiGeometry::Create(_object, true, true, true)（默认 uv=2/normal=3）：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1、uv@TEXCOORD2（binding1）、normal@TEXCOORD3（binding2）。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 aPos : TEXCOORD0;
    float4 aColor : TEXCOORD1;
    float2 aTexCoord : TEXCOORD2;
    float4 aNormal : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoords : TEXCOORD2;
    float4 FragPosLightSpace : TEXCOORD3;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.FragPos = mul(gModel, i.aPos).xyz;
    float3x3 nm = (float3x3)gModel;
    o.Normal = float4(mul(transpose(Mat3Inverse(nm)), i.aNormal.xyz), 0.0);
    o.TexCoords = i.aTexCoord;
    o.FragPosLightSpace = mul(gExtraMat4[0], float4(o.FragPos, 1.0));   // extraMat4[0]=lightSpaceMatrix
    o.sv = mul(gProjection, mul(gView, mul(gModel, i.aPos)));
    return o;
}
