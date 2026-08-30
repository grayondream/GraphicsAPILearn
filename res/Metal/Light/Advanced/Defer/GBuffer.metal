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
    float2 textureCoord [[attribute(2)]];
    float4 normal [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 FragPos;
    float2 TexCoords;
    float3 Normal;
};

vertex VertexOut Defer_GBuffer_vertex(VertexIn in [[stage_in]],
                                      constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    float4 worldPos = ubo.model * float4(in.pos);
    out.FragPos = worldPos.xyz; 
    out.TexCoords = in.textureCoord;
    
    float3x3 normalMatrix = transpose(mat3Inverse(float3x3(ubo.model[0].xyz, ubo.model[1].xyz, ubo.model[2].xyz)));
    out.Normal = normalMatrix * in.normal.xyz;

    out.position = ubo.projection * ubo.view * worldPos;
    return out;
}

struct GBufferOutput {
    float4 gPosition [[color(0)]];
    float4 gNormal [[color(1)]];
    float4 gAlbedoSpec [[color(2)]];
};

fragment GBufferOutput Defer_GBuffer_fragment(VertexOut in [[stage_in]],
                                              constant UniformBlock& ubo [[buffer(8)]],
                                              texture2d<float> diffuseTexture [[texture(0)]],
                                              sampler diffuseSampler [[sampler(0)]]) {
    GBufferOutput output;
    // store the fragment position vector in the first gbuffer texture
    output.gPosition = float4(in.FragPos, 1.0);
    // also store the per-fragment normals into the gbuffer
    output.gNormal = float4(normalize(in.Normal), 1.0);
    // and the diffuse per-fragment color
    output.gAlbedoSpec.rgb = diffuseTexture.sample(diffuseSampler, in.TexCoords).rgb;
    // store specular intensity in gAlbedoSpec's alpha component
    output.gAlbedoSpec.a = diffuseTexture.sample(diffuseSampler, in.TexCoords).r;
    return output;
}