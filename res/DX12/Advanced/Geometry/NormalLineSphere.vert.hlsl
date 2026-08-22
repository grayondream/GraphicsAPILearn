// 对应 res/GL/Advanced/Geometry/NormalLineSphere.vs：线框球体管线（PolygonMode::Line）。
// layout 同 NormalLine：binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1、normal@TEXCOORD2
// （shader 未消费 normal，不声明即不读）。
#include "../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 inColor : TEXCOORD1;
    float4 aNormal : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = mul(gProjection, mul(gView, mul(gModel, i.pos)));
    return o;
}
