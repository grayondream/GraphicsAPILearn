// 对应 res/GL/Advanced/Geometry/NormalLine.vs。
// layout 为 Sphere 的 RhiGeometry::Create(_object, false, true, true, Layout{0, 2})：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、normal@TEXCOORD2（binding2）。
// 注意 VS 只输出 view*model（无 projection），投影在 GS 内做；
// GLSL 局部变量 normalMatrix 改名 nm 避免与 cbuffer 槽 gNormalMatrix 混淆。
#include "../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 inColor : TEXCOORD1;
    float4 aNormal : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 normal : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = mul(gView, mul(gModel, i.pos));
    float3x3 nm = transpose(inverse(mul((float3x3)gView, (float3x3)gModel)));
    o.normal = normalize(mul(nm, i.aNormal.rgb).xyz);
    return o;
}
