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
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float2 textureCoord [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 FragPos;
    float2 TexCoords;
    float3 Normal;
};

vertex VertexOut SSAO_GBuffer_vertex(VertexIn in [[stage_in]],
                                     constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    float4 viewPos = ubo.view * ubo.model * float4(in.pos, 1.0);
    out.FragPos = viewPos.xyz; 
    out.TexCoords = in.textureCoord;
    
    float3x3 normalMatrix = transpose(inverse(float3x3(ubo.view * ubo.model)));
    out.Normal = normalMatrix * (ubo.floatPool[35] > 0.5 ? -in.normal : in.normal);   // invertedNormals
    
    out.position = ubo.projection * viewPos;
    return out;
}

struct GBufferOutput {
    float4 gPosition [[color(0)]];
    float4 gNormal [[color(1)]];
    float4 gAlbedo [[color(2)]];
};

fragment GBufferOutput SSAO_GBuffer_fragment(VertexOut in [[stage_in]],
                                             constant UniformBlock& ubo [[buffer(0)]]) {
    GBufferOutput output;
    // store the fragment position vector in the first gbuffer texture
    output.gPosition = float4(in.FragPos, 1.0);
    // also store the per-fragment normals into the gbuffer
    output.gNormal = float4(normalize(in.Normal), 1.0);
    // and the diffuse per-fragment color
    output.gAlbedo.rgb = float3(0.95);
    output.gAlbedo.a = 1.0;
    return output;
}