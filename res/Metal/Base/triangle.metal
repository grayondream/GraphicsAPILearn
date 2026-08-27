#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float4 pos [[attribute(0)]];
    float4 color [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float4 fragColor;
};

vertex VertexOut triangle_vertex(VertexIn in [[stage_in]]) {
    VertexOut out;
    out.position = in.pos;
    out.fragColor = in.color;
    return out;
}

fragment half4 triangle_fragment(VertexOut in [[stage_in]]) {
    return half4(in.fragColor);
}
