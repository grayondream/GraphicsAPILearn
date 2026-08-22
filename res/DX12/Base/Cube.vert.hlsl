// 顶点 layout 以 CubeApp 的 RhiGeometry::Create(Cube, useUv+useNormal) 为准：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、
// uv@TEXCOORD2（binding1）、normal@TEXCOORD3（binding2，光照前占位不消费）。
// 对应 res/GL/Base/Cube.vert（GLSL location0-3）。矩阵方向约定见 _uniform_block.hlsli：
// CPU 列主序直传 + HLSL column_major → 统一 mul(matrix, vector)。
#include "../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.sv = mul(gProjection, mul(gView, mul(gModel, i.pos)));
    o.uv = i.uv;
    return o;
}
