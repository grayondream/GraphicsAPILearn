// 对应 res/GL/Advanced/UniformBuffer/Cube.frag：cubeColor（vec4Pool[5]）调制。
#include "../../../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
    float4 fragColor : TEXCOORD0;
    float2 textureCoord : TEXCOORD1;
};

float4 PSMain(PSIn i) : SV_Target {
    return gVec4Pool[5] * i.fragColor;
}
