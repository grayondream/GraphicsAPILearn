// 光源立方管线以 SimpleLightDiffuseApp 的 RhiGeometry::Create(_object, false, false, true)
// 为准：binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32），无 uv/normal。
// 对应 res/GL/Light/Diffuse/Light.vert。
#include "../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
};

struct VSOut {
    float4 sv : SV_Position;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = mul(gProjection, mul(gView, mul(gModel, i.pos)));
    return o;
}
