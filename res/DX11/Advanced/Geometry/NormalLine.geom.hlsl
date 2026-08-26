// DX11/SM5.0 — NormalLine GS（TriangleStream thin-quad 模拟线段）。
// DX11 部分驱动对 GS LineStream 光栅化兼容性差（线条不渲染），
// 改用 TriangleStream 输出四边形条带（每法线线段 4 顶点 ≈ 1px 宽）。
// 对应 res/GL/Advanced/Geometry/NormalLine.gs 的 line_strip 视觉等价。
#include "../../_uniform_block.hlsli"

struct GSIn {
    float4 sv : SV_Position;
    float3 normal : TEXCOORD0;
};

struct GSOut {
    float4 sv : SV_Position;
};

static const float MAGNITUDE = 0.4;
static const float HALF_WIDTH = 0.002;

void GenerateQuad(GSIn v, inout TriangleStream<GSOut> stream)
{
    float4 a = mul(gProjection, v.sv);
    float4 b = mul(gProjection, float4(v.sv.xyz + v.normal * MAGNITUDE, 1.0));
    float2 ndcA = a.xy / a.w;
    float2 ndcB = b.xy / b.w;
    float2 dir = normalize(ndcB - ndcA);
    float2 perp = float2(-dir.y, dir.x) * HALF_WIDTH;
    GSOut o;
    o.sv = float4((ndcA + perp) * a.w, a.z, a.w); stream.Append(o);
    o.sv = float4((ndcA - perp) * a.w, a.z, a.w); stream.Append(o);
    o.sv = float4((ndcB + perp) * b.w, b.z, b.w); stream.Append(o);
    o.sv = float4((ndcB - perp) * b.w, b.z, b.w); stream.Append(o);
    stream.RestartStrip();
}

[maxvertexcount(12)]
void GSMain(triangle GSIn input[3], inout TriangleStream<GSOut> stream)
{
    GenerateQuad(input[0], stream);
    GenerateQuad(input[1], stream);
    GenerateQuad(input[2], stream);
}
