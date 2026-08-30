#include <metal_stdlib>
using namespace metal;

struct ULight {
    float4 position;
    float4 direction;
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float4 params;
};

struct UniformBlock {
    float4x4 projection;
    float4x4 view;
    float4x4 model;
    float4x4 normalMatrix;
    float4x4 viewModel;
    float4x4 extraMat4[14];
    float4 vec4Pool[64];
    float4 vec3Pool[64];
    float floatPool[64];
    ULight lights[256];
};

struct VertexIn {
    float4 pos [[attribute(0)]];
    float4 inColor [[attribute(1)]];
    float2 inTextureCoord [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 textureCoord;
    float4 color;
};

vertex VertexOut Screen_vertex(VertexIn in [[stage_in]],
                               constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.position = in.pos;
    out.textureCoord = in.inTextureCoord;
    out.color = in.inColor;
    return out;
}

constexpr sampler g_screenSampler(mag_filter::linear, min_filter::linear);

half4 screenOrigin(texture2d<half> tex, float2 uv) {
    return tex.sample(g_screenSampler, uv);
}

half4 screenInversion(texture2d<half> tex, float2 uv) {
    half4 color = tex.sample(g_screenSampler, uv);
    return half4(half3(1.0h - color.rgb), 1.0h);
}

half4 screenGray(texture2d<half> tex, float2 uv) {
    half4 color = tex.sample(g_screenSampler, uv);
    half average = 0.2126h * color.r + 0.7152h * color.g + 0.0722h * color.b;
    return half4(average, average, average, 1.0h);
}

half4 screenKernel(texture2d<half> tex, float2 uv) {
    const float offset = 1.0 / 300.0;
    float2 offsets[9] = {
        float2(-offset,  offset),
        float2( 0.0,     offset),
        float2( offset,  offset),
        float2(-offset,  0.0),
        float2( 0.0,     0.0),
        float2( offset,  0.0),
        float2(-offset, -offset),
        float2( 0.0,    -offset),
        float2( offset, -offset)
    };

    float kern[9] = {
        -1, -1, -1,
        -1,  9, -1,
        -1, -1, -1
    };

    half3 sampleTex[9];
    for (int i = 0; i < 9; i++) {
        sampleTex[i] = half3(tex.sample(g_screenSampler, uv + offsets[i]).rgb);
    }
    half3 col = half3(0.0);
    for (int i = 0; i < 9; i++) {
        col += sampleTex[i] * half(kern[i]);
    }
    return half4(col, 1.0h);
}

fragment half4 Screen_fragment(VertexOut in [[stage_in]],
                                constant UniformBlock& ubo [[buffer(8)]],
                                texture2d<half> textureSampler [[texture(0)]]) {
    half4 ocolor;
    if (ubo.floatPool[15] > 2.5) {
        ocolor = screenKernel(textureSampler, in.textureCoord);
    } else if (ubo.floatPool[15] > 1.5) {
        ocolor = screenGray(textureSampler, in.textureCoord);
    } else if (ubo.floatPool[15] > 0.5) {
        ocolor = screenInversion(textureSampler, in.textureCoord);
    } else {
        ocolor = screenOrigin(textureSampler, in.textureCoord);
    }
    return ocolor;
}