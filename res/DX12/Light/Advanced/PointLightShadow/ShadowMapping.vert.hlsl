// 对应 res/GL/Light/Advanced/PointLightShadow/ShadowMapping.vs。
// layout 为 Cube 的 RhiGeometry::Create(_object, true, true, true)（默认 uv=2/normal=3）：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1、uv@TEXCOORD2（binding1）、normal@TEXCOORD3（binding2）。
// reverse_normals=floatPool[34]：外侧大盒反转法线（从内部显示光照）的 hack 照抄。
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
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.FragPos = mul(gModel, i.aPos).xyz;
    float3 n = FPOOL(34) > 0.5 ? -i.aNormal.xyz : i.aNormal.xyz;   // reverse_normals
    float3x3 nm = transpose(Mat3Inverse((float3x3)gModel));
    o.Normal = mul(nm, n);
    o.TexCoords = i.aTexCoord;
    o.sv = mul(gProjection, mul(gView, mul(gModel, i.aPos)));
    return o;
}
