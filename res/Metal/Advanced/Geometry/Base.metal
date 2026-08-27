#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float2 aPos [[attribute(0)]];
    float3 aColor [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 color;
};

vertex VertexOut GeometryBase_vertex(VertexIn in [[stage_in]]) {
    VertexOut out;
    out.color = in.aColor;
    out.position = float4(in.aPos.x, in.aPos.y, 0.0, 1.0);
    return out;
}

fragment half4 GeometryBase_fragment(VertexOut in [[stage_in]]) {
    return half4(half3(in.color), 1.0h);
}