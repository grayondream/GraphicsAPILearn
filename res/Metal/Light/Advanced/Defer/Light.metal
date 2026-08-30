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
    float Radius;
};

struct VertexIn {
    float3 pos [[attribute(0)]];
    float2 textureCoord [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 TexCoords;
};

vertex VertexOut Defer_Light_vertex(VertexIn in [[stage_in]],
                                    constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.TexCoords = in.textureCoord;
    out.position = float4(in.pos, 1.0);
    return out;
}

fragment float4 Defer_Light_fragment(VertexOut in [[stage_in]],
                                     constant UniformBlock& ubo [[buffer(8)]],
                                     texture2d<float> gPosition [[texture(0)]],
                                     texture2d<float> gNormal [[texture(1)]],
                                     texture2d<float> gAlbedoSpec [[texture(2)]],
                                     sampler positionSampler [[sampler(0)]],
                                     sampler normalSampler [[sampler(1)]],
                                     sampler albedoSpecSampler [[sampler(2)]]) {
    const int NR_LIGHTS = 13 * 13;
    // retrieve data from gbuffer
    float3 FragPos = gPosition.sample(positionSampler, in.TexCoords).rgb;
    float3 Normal = gNormal.sample(normalSampler, in.TexCoords).rgb;
    float3 Diffuse = gAlbedoSpec.sample(albedoSpecSampler, in.TexCoords).rgb;
    float Specular = gAlbedoSpec.sample(albedoSpecSampler, in.TexCoords).a;
    
    // then calculate lighting as usual
    float3 lighting  = Diffuse * 0.1; // hard-coded ambient component
    float3 viewDir  = normalize(ubo.vec4Pool[0].xyz - FragPos);   // viewPos
    for(int i = 0; i < NR_LIGHTS; ++i)
    {
        Light light;
        light.Position = ubo.lights[i].position.xyz;
        light.Color = ubo.lights[i].diffuse.xyz;
        light.Linear = ubo.lights[i].params.y;
        light.Quadratic = ubo.lights[i].params.z;
        light.Radius = ubo.lights[i].direction.w;   // Radius 存 direction.w（与 quadratic/params.z 分离）

        float distance = 0;
        if(ubo.floatPool[46] > 0.5){   // enableVolume
            distance = length(light.Position - FragPos);
        }

        if(distance > light.Radius){
            continue;
        }

        {
            // diffuse
            float3 lightDir = normalize(light.Position - FragPos);
            float3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.Color;
            // specular
            float3 halfwayDir = normalize(lightDir + viewDir);  
            float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
            float3 specular = light.Color * spec * Specular;
            // attenuation
            float distance2 = length(light.Position - FragPos);
            float attenuation = 1.0 / (1.0 + light.Linear * distance2 + light.Quadratic * distance2 * distance2);
            diffuse *= attenuation;
            specular *= attenuation;
            lighting += diffuse + specular;        
        }
    }
    return float4(lighting, 1.0);
}