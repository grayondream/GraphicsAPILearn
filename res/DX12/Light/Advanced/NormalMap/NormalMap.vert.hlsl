// 顶点布局以 NormalMapApp::CreateRectBuffer 为准：binding0 单一交错缓冲 stride 56：
// pos@TEXCOORD0(Float3,0) + normal@TEXCOORD1(Float3,12) + texCoords@TEXCOORD2(Float2,24)
// + tangent@TEXCOORD3(Float3,32) + bitangent@TEXCOORD4(Float3,44)。
// 对应 res/GL/Light/Advanced/NormalMap/NormalMap.vs。VS_OUT block 摊平为 TEXCOORD 插值器；
// lightPos→vec4Pool[2]、viewPos→vec4Pool[0]。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float3 aPos : TEXCOORD0;
    float3 aNormal : TEXCOORD1;
    float2 aTexCoords : TEXCOORD2;
    float3 aTangent : TEXCOORD3;
    float3 aBitangent : TEXCOORD4;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float2 TexCoords : TEXCOORD1;
    float3 TangentLightPos : TEXCOORD2;
    float3 TangentViewPos : TEXCOORD3;
    float3 TangentFragPos : TEXCOORD4;
    float3 aNormal : TEXCOORD5;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.FragPos = mul(gModel, float4(i.aPos, 1.0)).xyz;
    o.TexCoords = i.aTexCoords;

    float3x3 nm = transpose(Mat3Inverse((float3x3)gModel));
    float3 T = normalize(mul(nm, i.aTangent));
    float3 N = normalize(mul(nm, i.aNormal));
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = transpose(float3x3(T, B, N));
    o.TangentLightPos = mul(TBN, gVec4Pool[2].xyz);
    o.TangentViewPos = mul(TBN, gVec4Pool[0].xyz);
    o.TangentFragPos = mul(TBN, o.FragPos);
    o.aNormal = i.aNormal;
    o.sv = mul(gProjection, mul(gView, mul(gModel, float4(i.aPos, 1.0))));
    return o;
}
