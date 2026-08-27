#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float4 pos [[attribute(0)]];
    float4 color [[attribute(1)]];
    float2 inTextureCoord [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float4 fragColor;
    float2 textureCoord;
};

vertex VertexOut SimpleTexture_vertex(VertexIn in [[stage_in]]) {
    VertexOut out;
    out.position = in.pos;
    out.fragColor = in.color;
    out.textureCoord = in.inTextureCoord;
    return out;
}

fragment half4 SimpleTexture_fragment(VertexOut in [[stage_in]],
                                      texture2d<half> textureSampler [[texture(0)]]) {
    constexpr sampler s(mag_filter::linear, min_filter::linear);
    half4 texColor = textureSampler.sample(s, in.textureCoord);
    return texColor * half4(in.fragColor);
}
