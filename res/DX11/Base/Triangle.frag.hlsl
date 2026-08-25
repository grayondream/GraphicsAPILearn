#include "../_uniform_block.hlsli"

// DX11/SM5.0 起步文件（自 res/DX12/Base 同名拷贝）：直接输出插值顶点色。
struct PSIn {
    float4 sv : SV_Position;
    float4 col : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    return i.col;
}
