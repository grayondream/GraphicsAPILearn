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

vertex VertexOut SSAO_SSAO_vertex(VertexIn in [[stage_in]],
                                  constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.TexCoords = in.textureCoord;
    out.position = float4(in.pos, 1.0);
    return out;
}

fragment float SSAO_SSAO_fragment(VertexOut in [[stage_in]],
                                  constant UniformBlock& ubo [[buffer(0)]],
                                  texture2d<float> gPosition [[texture(0)]],
                                  texture2d<float> gNormal [[texture(1)]],
                                  texture2d<float> texNoise [[texture(2)]],
                                  sampler positionSampler [[sampler(0)]],
                                  sampler normalSampler [[sampler(1)]],
                                  sampler noiseSampler [[sampler(2)]]) {
    // parameters (you'd probably want to use them as uniforms to more easily tweak the effect)
    int kernelSize = 64;
    float radius = 0.5;
    float bias = 0.025;

    // tile noise texture over screen based on screen dimensions divided by noise size
    const float2 noiseScale = float2(800.0/4.0, 600.0/4.0); 

    // get input for SSAO algorithm
    float3 fragPos = gPosition.sample(positionSampler, in.TexCoords).xyz;
    float3 normal = normalize(gNormal.sample(normalSampler, in.TexCoords).rgb);
    float3 randomVec = normalize(texNoise.sample(noiseSampler, in.TexCoords * noiseScale).xyz);
    // create TBN change-of-basis matrix: from tangent-space to view-space
    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);
    // iterate over the sample kernel and calculate occlusion factor
    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        // get sample position
        float3 samplePos = TBN * ubo.vec3Pool[i].xyz; // samples[i] → vec3Pool[0..63], from tangent to view-space
        samplePos = fragPos + samplePos * radius; 
        
        // project sample position (to sample texture) (to get position on screen/texture)
        float4 offset = float4(samplePos, 1.0);
        offset = ubo.projection * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        
        // get sample depth
        float sampleDepth = gPosition.sample(positionSampler, offset.xy).z; // get depth value of kernel sample
        
        // range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;           
    }
    occlusion = 1.0 - (occlusion / kernelSize);
    
    return occlusion;
}