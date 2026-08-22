// 对应 res/GL/Advanced/FrameBuffer/Basic.vert。
// layout 为 Cube 的 RhiGeometry::Create(_object, true, false, true)（默认 uv=2）：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、uv@TEXCOORD2（binding1）。
#include "../../../_uniform_block.hlsli"

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
