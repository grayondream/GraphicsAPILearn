#include <metal_stdlib>
using namespace metal;
// __MAT_HELPERS__ (auto-added: MSL lacks inverse()/mat4(mat3))
float3x3 mat3Inverse(float3x3 m) {
    float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2];
    float a10 = m[1][0], a11 = m[1][1], a12 = m[1][2];
    float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2];
    float b01 =  a22*a11 - a12*a21;
    float b11 = -a22*a10 + a12*a20;
    float b21 =  a21*a10 - a11*a20;
    float det = a00*b01 + a01*b11 + a02*b21;
    float id = 1.0 / det;
    return float3x3(
        b01*id, (-a22*a01 + a02*a21)*id, ( a12*a01 - a02*a11)*id,
        b11*id, ( a22*a00 - a02*a20)*id, (-a12*a00 + a02*a10)*id,
        b21*id, (-a21*a00 + a01*a20)*id, ( a11*a00 - a01*a10)*id
    );
}
float4x4 mat4FromMat3(float3x3 m) {
    return float4x4(float4(m[0], 0.0), float4(m[1], 0.0), float4(m[2], 0.0), float4(0.0, 0.0, 0.0, 1.0));
}

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
    float4 aNormal [[attribute(2)]];
    float2 inTextureCoord [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 textureCoord;
    float4 fragColor;
    float4 normal;
    float4 position_world;
};

vertex VertexOut SkyBoxCube_vertex(VertexIn in [[stage_in]],
                                   constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.position = ubo.projection * ubo.view * ubo.model * in.pos;
    out.textureCoord = in.inTextureCoord;
    out.fragColor = in.inColor;
    float3x3 model3x3 = float3x3(ubo.model.columns[0].xyz,
                                   ubo.model.columns[1].xyz,
                                   ubo.model.columns[2].xyz);
    float3x3 normalMat = transpose(mat3Inverse(model3x3));
    out.normal = float4(normalMat * in.aNormal.xyz, 1.0);
    out.position_world = float4(model3x3 * in.pos.xyz, 1.0);
    return out;
}

fragment half4 SkyBoxCube_fragment(VertexOut in [[stage_in]],
                                   constant UniformBlock& ubo [[buffer(8)]],
                                   texture2d<half> textureSampler [[texture(0)]],
                                   texturecube<float> skyBoxSampler [[texture(1)]]) {
    constexpr sampler s(mag_filter::linear, min_filter::linear);

    if (ubo.floatPool[36] > 0.5) {
        float3 I = normalize(in.position_world.xyz - ubo.vec4Pool[1].xyz);
        float3 R = reflect(I, normalize(in.normal.xyz));
        return half4(skyBoxSampler.sample(s, R));
    } else if (ubo.floatPool[37] > 0.5) {
        float ratio = 1.00 / 1.52;
        float3 I = normalize(in.position_world.xyz - ubo.vec4Pool[1].xyz);
        float3 R = refract(I, normalize(in.normal.xyz), ratio);
        return half4(skyBoxSampler.sample(s, R));
    }

    return textureSampler.sample(s, in.textureCoord);
}