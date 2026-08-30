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
};

vertex VertexOut Gamma_Object_vertex(VertexIn in [[stage_in]],
                                     constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.FragPos = in.pos.xyz;
    out.Normal = in.normal.xyz;
    out.TexCoords = in.textureCoord;
    out.position = ubo.projection * ubo.view * in.pos;
    return out;
}

float3 BlinnPhong(float3 normal, float3 fragPos, float3 lightPos, float3 lightColor, constant UniformBlock& ubo){
    // diffuse
    float3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * lightColor;
    // specular
    float3 viewDir = normalize(ubo.vec4Pool[0].xyz - fragPos);   // viewPos
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = 0.0;
    float3 halfwayDir = normalize(lightDir + viewDir);  
    spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    float3 specular = spec * lightColor;    
    // simple attenuation
    float max_distance = 1.5;
    float distance = length(lightPos - fragPos);
    float attenuation = 1.0 / (ubo.floatPool[20] > 0.5 ? distance * distance : distance);   // enableGamma
    
    diffuse *= attenuation;
    specular *= attenuation;
    
    return diffuse + specular;
}

fragment float4 Gamma_Object_fragment(VertexOut in [[stage_in]],
                                      constant UniformBlock& ubo [[buffer(8)]],
                                      texture2d<float> textureSampler [[texture(0)]],
                                      sampler sampler2D [[sampler(0)]]) {
    float3 color = textureSampler.sample(sampler2D, in.TexCoords).rgb;
    float3 lighting = float3(0.0);
    for(int i = 0; i < 5; ++i)
        lighting += BlinnPhong(normalize(in.Normal), in.FragPos, ubo.vec4Pool[13 + i].xyz, ubo.vec4Pool[29 + i].xyz, ubo);
    color *= lighting;
    if(ubo.floatPool[20] > 0.5){   // enableGamma
        color = pow(color, float3(1.0/ubo.floatPool[5]));   // gammaValue
    }
        
    return float4(color, 1.0);
}