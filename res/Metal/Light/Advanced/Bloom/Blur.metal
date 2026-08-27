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
    float2 textureCoord [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 TexCoords;
};

vertex VertexOut Bloom_Blur_vertex(VertexIn in [[stage_in]],
                                   constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.TexCoords = in.textureCoord;
    out.position = float4(in.pos, 1.0);
    return out;
}

fragment float4 Bloom_Blur_fragment(VertexOut in [[stage_in]],
                                    constant UniformBlock& ubo [[buffer(0)]],
                                    texture2d<float> image [[texture(0)]],
                                    sampler imageSampler [[sampler(0)]]) {
    const float weight[5] = {0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162};

     float2 tex_offset = 1.0 / float2(image.get_width(), image.get_height()); // gets size of single texel
     float3 result = image.sample(imageSampler, in.TexCoords).rgb * weight[0];
     if(ubo.floatPool[12] > 0.5)   // horizontal
     {
         for(int i = 1; i < 5; ++i)
         {
            result += image.sample(imageSampler, in.TexCoords + float2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += image.sample(imageSampler, in.TexCoords - float2(tex_offset.x * i, 0.0)).rgb * weight[i];
         }
     }
     else
     {
         for(int i = 1; i < 5; ++i)
         {
             result += image.sample(imageSampler, in.TexCoords + float2(0.0, tex_offset.y * i)).rgb * weight[i];
             result += image.sample(imageSampler, in.TexCoords - float2(0.0, tex_offset.y * i)).rgb * weight[i];
         }
     }
     return float4(result, 1.0);
}