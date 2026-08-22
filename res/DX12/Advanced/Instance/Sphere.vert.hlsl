// 对应 res/GL/Advanced/Instance/Sphere.vs：PerInstance 偏移实例化。
// layout 以 MultieInstanceApp 为准：binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、
// normal@TEXCOORD2（binding2）、aOffset(Float2)@TEXCOORD3（binding3，PerInstance stride 8）；
// gl_InstanceID → SV_InstanceID（uint）。
#include "../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 inColor : TEXCOORD1;
    float4 aNormal : TEXCOORD2;
    float2 aOffset : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
    float4 color : TEXCOORD0;
};

VSOut VSMain(VSIn i, uint svInstanceID : SV_InstanceID) {
    VSOut o;
    int instanceIndex = (int)svInstanceID;
    float c = instanceIndex * 5.0 / 255;
    o.color = float4(c, 0.0, 0.0, 1.0); // Red color

    float4 position = i.pos * (svInstanceID / 100.0) + float4(i.aOffset, 0.0, 1.0);
    o.sv = mul(gProjection, mul(gView, mul(gModel, position)));
    return o;
}
