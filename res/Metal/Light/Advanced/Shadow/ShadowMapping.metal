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
    float3 Normal;
    float2 TexCoords;
    float4 FragPosLightSpace;
};

vertex VertexOut Shadow_ShadowMapping_vertex(VertexIn in [[stage_in]],
                                             constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.FragPos = float3(ubo.model * in.pos);
    out.Normal = transpose(inverse(float3x3(ubo.model))) * in.normal.xyz;
    out.TexCoords = in.textureCoord;
    out.FragPosLightSpace = ubo.extraMat4[0] * float4(out.FragPos, 1.0);
    out.position = ubo.projection * ubo.view * ubo.model * in.pos;
    return out;
}

float ShadowCalculation(float4 fragPosLightSpace, float3 FragPos, float3 Normal, constant UniformBlock& ubo, texture2d<float> shadowMap, sampler shadowSampler) {
    // perform perspective divide
    float3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = shadowMap.sample(shadowSampler, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    float shadow = 0.0;
    float bias = 0.0;
    if(ubo.floatPool[25] > 0.5){
        float3 normal = normalize(Normal);
        float3 lightDir = normalize(ubo.vec4Pool[2].xyz - FragPos);
        bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
        shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;
    }else{
        shadow = currentDepth > closestDepth  ? 1.0 : 0.0;
    }
    
    if(projCoords.z > 1.0)
        shadow = 0.0;

    if(ubo.floatPool[40] > 0.5){
        float2 texelSize = 1.0 / float2(shadowMap.get_width(), shadowMap.get_height());
        for(int x = -1; x <= 1; ++x)
        {
            for(int y = -1; y <= 1; ++y)
            {
                float pcfDepth = shadowMap.sample(shadowSampler, projCoords.xy + float2(x, y) * texelSize).r; 
                shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
            }    
        }
        shadow /= 9.0;
    }
    

    return shadow;
}

fragment float4 Shadow_ShadowMapping_fragment(VertexOut in [[stage_in]],
                                              constant UniformBlock& ubo [[buffer(0)]],
                                              texture2d<float> diffuseTexture [[texture(0)]],
                                              texture2d<float> shadowMap [[texture(1)]],
                                              sampler diffuseSampler [[sampler(0)]],
                                              sampler shadowSampler [[sampler(1)]]) {
    float3 color = diffuseTexture.sample(diffuseSampler, in.TexCoords).rgb;
    float3 normal = normalize(in.Normal);
    float3 lightColor = float3(1.0);
    // ambient
    float3 ambient = 0.3 * lightColor;
    // diffuse
    float3 lightDir = normalize(ubo.vec4Pool[2].xyz - in.FragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * lightColor;
    // specular
    float3 viewDir = normalize(ubo.vec4Pool[0].xyz - in.FragPos);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = 0.0;
    float3 halfwayDir = normalize(lightDir + viewDir);  
    spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    float3 specular = spec * lightColor;    
    // calculate shadow
    float shadow = ShadowCalculation(in.FragPosLightSpace, in.FragPos, in.Normal, ubo, shadowMap, shadowSampler);                      
    float3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;    
    
    float4 FragColor = float4(lighting, 1.0);
    if(ubo.floatPool[15] > 1.5){
        FragColor = float4(lightColor, 1.0);
    }
    
    if(ubo.floatPool[18] > 0.5){
        if(ubo.floatPool[15] > 0.5 && ubo.floatPool[15] <= 1.5){
            FragColor = float4(1.0, 0.0, 0.0, 1.0);
        }else if(ubo.floatPool[15] > 1.5){
            FragColor = float4(1.0, 1.0, 1.0, 1.0);
        }else{
            FragColor = float4(0.0, 1.0, 0.0, 1.0);
        }
    }
    
    return FragColor;
}