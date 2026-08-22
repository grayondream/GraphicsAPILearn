// 对应 res/GL/Advanced/SkyBox/Cube.vert。
// layout 以 SkyboxApp 的 RhiGeometry::Create(_object, true, true, true, Layout{3, 2}) 为准：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、normal@TEXCOORD2（binding2）、
// uv@TEXCOORD3（binding1）。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 inColor : TEXCOORD1;
    float4 aNormal : TEXCOORD2;
    float2 inTextureCoord : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
    float2 textureCoord : TEXCOORD0;
    float4 fragColor : TEXCOORD1;
    float4 normal : TEXCOORD2;
    float4 position : TEXCOORD3;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = mul(gProjection, mul(gView, mul(gModel, i.pos)));
    o.textureCoord = i.inTextureCoord;
    o.fragColor = i.inColor;
    float3x3 v3model = (float3x3)gModel;
    o.normal = float4(mul(transpose(inverse(v3model)), i.aNormal.xyz), 1.0);
    o.position = float4(mul(v3model, i.pos.xyz), 1.0);
    return o;
}
