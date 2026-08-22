// 对应 res/GL/Advanced/FrameBuffer/Screen.vert：屏幕 quad，pos 直接作 clip 坐标。
// layout 为 Rect 的 RhiGeometry::Create(_object, true, false, true)（默认 uv=2）：
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
    float4 color : TEXCOORD1;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = i.pos;
    o.textureCoord = i.inTextureCoord;
    o.color = i.inColor;
    return o;
}
