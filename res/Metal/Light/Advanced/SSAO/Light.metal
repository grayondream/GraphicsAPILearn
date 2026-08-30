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
    
    float Linear;
    float Quadratic;
};

struct VertexIn {
    float3 pos [[attribute(0)]];
    float2 textureCoord [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 TexCoords;
};

vertex VertexOut SSAO_Light_vertex(VertexIn in [[stage_in]],
                                   constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.TexCoords = in.textureCoord;
    out.position = float4(in.pos, 1.0);
    return out;
}

fragment float4 SSAO_Light_fragment(VertexOut in [[stage_in]],
                                    constant UniformBlock& ubo [[buffer(8)]],
                                    texture2d<float> gPosition [[texture(0)]],
                                    texture2d<float> gNormal [[texture(1)]],
                                    texture2d<float> gAlbedo [[texture(2)]],
                                    texture2d<float> ssao [[texture(3)]],
                                    sampler positionSampler [[sampler(0)]],
                                    sampler normalSampler [[sampler(1)]],
                                    sampler albedoSampler [[sampler(2)]],
                                    sampler ssaoSampler [[sampler(3)]]) {
    // retrieve data from gbuffer
    float3 FragPos = gPosition.sample(positionSampler, in.TexCoords).rgb;
    float3 Normal = gNormal.sample(normalSampler, in.TexCoords).rgb;
    float3 Diffuse = gAlbedo.sample(albedoSampler, in.TexCoords).rgb;
    float AmbientOcclusion = ubo.floatPool[39] > 0.5 ? ssao.sample(ssaoSampler, in.TexCoords).r : 1.0;   // enableSSAO

    Light light;
    light.Position = ubo.lights[0].position.xyz;
    light.Color = ubo.lights[0].diffuse.xyz;
    light.Linear = ubo.lights[0].params.y;
    light.Quadratic = ubo.lights[0].params.z;

    // then calculate lighting as usual
    float3 ambient = float3(0.3 * Diffuse * AmbientOcclusion);
    float3 lighting  = ambient; 
    float3 viewDir  = normalize(-FragPos); // viewpos is (0.0.0)
    // diffuse
    float3 lightDir = normalize(light.Position - FragPos);
    float3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.Color;
    // specular
    float3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(Normal, halfwayDir), 0.0), 8.0);
    float3 specular = light.Color * spec;
    // attenuation
    float distance = length(light.Position - FragPos);
    float attenuation = 1.0 / (1.0 + light.Linear * distance + light.Quadratic * distance * distance);
    diffuse *= attenuation;
    specular *= attenuation;
    lighting += diffuse + specular;

    return float4(lighting, 1.0);
}