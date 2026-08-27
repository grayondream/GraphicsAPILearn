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
                                      constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    float4 worldPos = ubo.model * float4(in.pos);
    out.FragPos = worldPos.xyz; 
    out.TexCoords = in.textureCoord;
    
    float3x3 normalMatrix = transpose(inverse(float3x3(ubo.model)));
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
                                              constant UniformBlock& ubo [[buffer(0)]],
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