// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T gs_5_0 编译，入口 GSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/Geometry/Explode.gs：triangles → triangle_strip（3 顶点爆炸）。
// GS 阶段访问 UniformBlock（b0，根签名 CBV visibility=ALL）；法线由三角面几何求取；
// time=floatPool[10] 驱动 sin 爆炸幅度。
#include "../../_uniform_block.hlsli"

struct GSIn {
    float4 sv : SV_Position;
    float4 color : TEXCOORD0;
};

struct GSOut {
    float4 sv : SV_Position;
    float4 fColor : TEXCOORD0;
};

float4 explode(float4 position, float3 normal)
{
    float magnitude = 2.0;
    float3 direction = normal * ((sin(FPOOL(10)) + 1.0) / 2.0) * magnitude;   // time
    return position + float4(direction, 0.0);
}

float3 GetNormal(float4 p0, float4 p1, float4 p2)
{
    float3 a = p0.xyz - p1.xyz;
    float3 b = p2.xyz - p1.xyz;
    return normalize(cross(a, b));
}

[maxvertexcount(3)]
void GSMain(triangle GSIn input[3], inout TriangleStream<GSOut> stream)
{
    GSOut o;
    float3 normal = GetNormal(input[0].sv, input[1].sv, input[2].sv);

    o.sv = explode(input[0].sv, normal);
    o.fColor = input[0].color;
    stream.Append(o);
    o.sv = explode(input[1].sv, normal);
    o.fColor = input[1].color;
    stream.Append(o);
    o.sv = explode(input[2].sv, normal);
    o.fColor = input[2].color;
    stream.Append(o);
    stream.RestartStrip();
}
