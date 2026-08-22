// 光源立方体管线：binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）。
// 对应 res/GL/Light/LightMap/Light.vert（GLSL location0/1）。矩阵方向约定见 _uniform_block.hlsli：
// CPU 列主序直传 + HLSL column_major → 统一 mul(matrix, vector)。
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
