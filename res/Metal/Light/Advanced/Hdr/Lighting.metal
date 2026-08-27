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
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float2 textureCoord [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 FragPos;
    float3 Normal;
    float2 TexCoords;
};

vertex VertexOut Hdr_Lighting_vertex(VertexIn in [[stage_in]],
                                     constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.FragPos = float3(ubo.model * float4(in.pos, 1.0));   
    out.TexCoords = in.textureCoord;
    
    float3 n = ubo.floatPool[34] > 0.5 ? -in.normal : in.normal;   // inverse_normals
    
    float3x3 normalMatrix = transpose(inverse(float3x3(ubo.model)));
    out.Normal = normalize(normalMatrix * n);
    
    out.position = ubo.projection * ubo.view * ubo.model * float4(in.pos, 1.0);
    return out;
}

fragment float4 Hdr_Lighting_fragment(VertexOut in [[stage_in]],
                                      constant UniformBlock& ubo [[buffer(0)]],
                                      texture2d<float> diffuseTexture [[texture(0)]],
                                      sampler diffuseSampler [[sampler(0)]]) {
    float3 color = diffuseTexture.sample(diffuseSampler, in.TexCoords).rgb;
    float3 normal = normalize(in.Normal);
    // ambient
    float3 ambient = 0.0 * color;
    // lighting
    float3 lighting = float3(0.0);
    for(int i = 0; i < 4; i++)
    {
        Light light;
        light.Position = ubo.lights[i].position.xyz;
        light.Color = ubo.lights[i].diffuse.xyz;
        // diffuse
        float3 lightDir = normalize(light.Position - in.FragPos);
        float diff = max(dot(lightDir, normal), 0.0);
        float3 diffuse = light.Color * diff * color;      
        float3 result = diffuse;        
        // attenuation (use quadratic as we have gamma correction)
        float distance = length(in.FragPos - light.Position);
        result *= 1.0 / (distance * distance);
        lighting += result;
                
    }
    return float4(ambient + lighting, 1.0);
}