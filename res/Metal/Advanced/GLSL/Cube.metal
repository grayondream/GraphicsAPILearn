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
    float4 fragColor;
};

vertex VertexOut GLSLCube_vertex(VertexIn in [[stage_in]],
                                 constant UniformBlock& ubo [[buffer(8)]],
                                 uint vertexID [[vertex_id]]) {
    VertexOut out;
    out.position = ubo.projection * ubo.view * ubo.model * in.pos;
    out.textureCoord = in.inTextureCoord;

    if (ubo.floatPool[43] > 0.5) {
        out.fragColor = float4(0.0, 1.0, 1.0, 1.0);
    } else {
        out.fragColor = in.inColor;
    }

    if (ubo.floatPool[42] > 0.5) {
        out.fragColor = float4(float(vertexID % 2), float(vertexID % 3), float(vertexID % 4), 1.0);
    }

    return out;
}

fragment half4 GLSLCube_fragment(VertexOut in [[stage_in]],
                                 constant UniformBlock& ubo [[buffer(8)]],
                                 texture2d<half> textureSampler [[texture(0)]]) {
    half4 color = half4(in.fragColor);

    if (ubo.floatPool[38] > 0.5) {
        if (in.position.x < 400) {
            color = half4(1.0h, 0.0h, 0.0h, 1.0h);
        } else {
            color = half4(0.0h, 1.0h, 0.0h, 1.0h);
        }
    }

    if (ubo.floatPool[44] > 0.5) {
        if (in.position.y > 0.0) {
            color = half4(0.0h, 0.0h, 1.0h, 1.0h);
        } else {
            color = half4(1.0h, 1.0h, 0.0h, 1.0h);
        }
    }

    return color;
}