// 对应 res/GL/Advanced/Geometry/Base.gs：points → triangle_strip（5 顶点小房子）。
// GLSL EmitVertex/EndPrimitive → TriangleStream.Append/RestartStrip；
// gl_in[0].gl_Position → 输入 SV_Position；gs_in[0].color → TEXCOORD0。
// dxc 已验证支持 [shader("geometry")] gs_6_0（Task 1 结论）。
struct GSIn {
    float4 sv : SV_Position;
    float3 color : TEXCOORD0;
};

struct GSOut {
    float4 sv : SV_Position;
    float3 fColor : TEXCOORD0;
};

[maxvertexcount(5)]
void GSMain(point GSIn input[1], inout TriangleStream<GSOut> stream)
{
    GSOut o;
    o.fColor = input[0].color;   // gs_in[0] since there's only one input vertex

    float4 position = input[0].sv;
    o.sv = position + float4(-0.2, -0.2, 0.0, 0.0);   // 1:bottom-left
    stream.Append(o);
    o.sv = position + float4(0.2, -0.2, 0.0, 0.0);    // 2:bottom-right
    stream.Append(o);
    o.sv = position + float4(-0.2, 0.2, 0.0, 0.0);    // 3:top-left
    stream.Append(o);
    o.sv = position + float4(0.2, 0.2, 0.0, 0.0);     // 4:top-right
    stream.Append(o);
    o.sv = position + float4(0.0, 0.4, 0.0, 0.0);     // 5:top
    o.fColor = float3(1.0, 1.0, 1.0);
    stream.Append(o);
    stream.RestartStrip();
}
