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

struct Light {
    float3 Position;
    float3 Color;
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
    float3 Normal;
    float2 TexCoords;
};

vertex VertexOut Bloom_Bloom_vertex(VertexIn in [[stage_in]],
                                    constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.FragPos = float3(ubo.model * float4(in.pos));   
    out.TexCoords = in.textureCoord;
        
    float3x3 normalMatrix = transpose(inverse(float3x3(ubo.model)));
    out.Normal = normalize(normalMatrix * float3(in.normal));
    
    out.position = ubo.projection * ubo.view * ubo.model * float4(in.pos);
    return out;
}

fragment float4 Bloom_Bloom_fragment(VertexOut in [[stage_in]],
                                     constant UniformBlock& ubo [[buffer(0)]],
                                     texture2d<float> diffuseTexture [[texture(0)]],
                                     sampler diffuseSampler [[sampler(0)]]) {
    float3 color = diffuseTexture.sample(diffuseSampler, in.TexCoords).rgb;
    float3 normal = normalize(in.Normal);
    // ambient
    float3 ambient = 0.1 * color;
    // lighting
    float3 lighting = float3(0.0);
    float3 viewDir = normalize(ubo.vec4Pool[0].xyz - in.FragPos);   // viewPos
    for(int i = 0; i < 4; i++)
    {
        Light light;
        light.Position = ubo.lights[i].position.xyz;
        light.Color = ubo.lights[i].diffuse.xyz;
        // diffuse
        float3 lightDir = normalize(light.Position - in.FragPos);
        float diff = max(dot(lightDir, normal), 0.0);
        float3 result = light.Color * diff * color;      
        // attenuation (use quadratic as we have gamma correction)
        float distance = length(in.FragPos - light.Position);
        result *= 1.0 / (distance * distance);
        lighting += result;
                
    }
    float3 result = ambient + lighting;
    // check whether result is higher than some threshold, if so, output as bloom threshold color
    float brightness = dot(result, float3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        return float4(result, 1.0);
    else
        return float4(0.0, 0.0, 0.0, 1.0);
    return float4(result, 1.0);
}