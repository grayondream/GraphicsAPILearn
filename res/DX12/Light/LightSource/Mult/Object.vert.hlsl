// 物体管线以 LightSourceMult 的 RhiGeometry::Create(_object, uv+normal+index,
// {.uvLocation=3, .normalLocation=2}) 为准：binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1
// （stride 32）、normal@TEXCOORD2（binding2）、uv@TEXCOORD3（binding1）。
// 对应 res/GL/Light/LightSource/Mult/Object.vert。GLSL 死接口 objectColor 未搬运。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
    float4 normal : TEXCOORD2;
    float2 uv : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
    float4 normal : TEXCOORD0;
    float4 fragPos : TEXCOORD1;
    float2 textureCoord : TEXCOORD2;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.fragPos = mul(gModel, i.pos);
    o.sv = mul(gProjection, mul(gView, o.fragPos));
    o.normal = mul(transpose(Mat3Inverse(gModel)), i.normal);
    o.textureCoord = i.uv;
    return o;
}
