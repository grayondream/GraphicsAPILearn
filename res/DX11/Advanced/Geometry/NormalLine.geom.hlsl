// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T gs_5_0 编译，入口 GSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/Geometry/NormalLine.gs：triangles → line_strip（6 顶点法线线）。
// 投影矩阵在 GS 内应用（VS 只输出 view*model）；MAGNITUDE=0.4 常量直译。
#include "../../_uniform_block.hlsli"

struct GSIn {
    float4 sv : SV_Position;
    float3 normal : TEXCOORD0;
};

struct GSOut {
    float4 sv : SV_Position;
};

static const float MAGNITUDE = 0.4;

void GenerateLine(GSIn v, inout LineStream<GSOut> stream)
{
    GSOut o;
    o.sv = mul(gProjection, v.sv);
    stream.Append(o);
    o.sv = mul(gProjection, float4(v.sv.xyz + v.normal * MAGNITUDE, 1.0));
    stream.Append(o);
    stream.RestartStrip();
}

[maxvertexcount(6)]
void GSMain(triangle GSIn input[3], inout LineStream<GSOut> stream)
{
    GenerateLine(input[0], stream);   // 第一个顶点法线
    GenerateLine(input[1], stream);   // 第二个顶点法线
    GenerateLine(input[2], stream);   // 第三个顶点法线
}
