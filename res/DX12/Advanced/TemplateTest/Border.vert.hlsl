// 对应 res/GL/Advanced/TemplateTest/Border.vert（与 Basic.vert 同构：模板描边放大盒）。
#include "../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 inColor : TEXCOORD1;
    float2 inTextureCoord : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
    float2 textureCoord : TEXCOORD0;
    float4 fragColor : TEXCOORD1;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = mul(gProjection, mul(gView, mul(gModel, i.pos)));
    o.textureCoord = i.inTextureCoord;
    o.fragColor = i.inColor;
    return o;
}
