#include "../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
    float4 col : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    return i.col;
}
