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
    float3 tangent [[attribute(3)]];
    float3 bitangent [[attribute(4)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 FragPos;
    float2 TexCoords;
    float3 TangentLightPos;
    float3 TangentViewPos;
    float3 TangentFragPos;
};

vertex VertexOut ParallaxMap_ParallaxMap_vertex(VertexIn in [[stage_in]],
                                               constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.FragPos = float3(ubo.model * float4(in.pos, 1.0));   
    out.TexCoords = in.textureCoord;   
    
    float3 T = normalize(float3x3(ubo.model) * in.tangent);
    float3 B = normalize(float3x3(ubo.model) * in.bitangent);
    float3 N = normalize(float3x3(ubo.model) * in.normal);
    float3x3 TBN = transpose(float3x3(T, B, N));

    out.TangentLightPos = TBN * ubo.vec4Pool[2].xyz;
    out.TangentViewPos  = TBN * ubo.vec4Pool[0].xyz;
    out.TangentFragPos  = TBN * out.FragPos;
    
    out.position = ubo.projection * ubo.view * ubo.model * float4(in.pos, 1.0);
    return out;
}

float2 ParallaxMapping(float2 texCoords, float3 viewDir, texture2d<float> depthMap, sampler depthSampler, constant UniformBlock& ubo) { 
    float height = depthMap.sample(depthSampler, texCoords).r;     
    return texCoords - viewDir.xy * (height * ubo.floatPool[6]);        
}

float2 ParallaxMappingSteep(float2 texCoords, float3 viewDir, texture2d<float> depthMap, sampler depthSampler, constant UniformBlock& ubo) { 
    // number of depth layers
    const float minLayers = 8;
    const float maxLayers = 32;
    float numLayers = mix(maxLayers, minLayers, abs(dot(float3(0.0, 0.0, 1.0), viewDir)));  
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    float2 P = viewDir.xy / viewDir.z * ubo.floatPool[6]; 
    float2 deltaTexCoords = P / numLayers;
  
    // get initial values
    float2  currentTexCoords     = texCoords;
    float currentDepthMapValue = depthMap.sample(depthSampler, currentTexCoords).r;
      
    while(currentLayerDepth < currentDepthMapValue)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = depthMap.sample(depthSampler, currentTexCoords).r;  
        // get depth of next layer
        currentLayerDepth += layerDepth;  
    }
    
    if(ubo.floatPool[24] > 0.5){
        float2 prevTexCoords = currentTexCoords + deltaTexCoords;

        // get depth after and before collision for linear interpolation
        float afterDepth  = currentDepthMapValue - currentLayerDepth;
        float beforeDepth = depthMap.sample(depthSampler, prevTexCoords).r - currentLayerDepth + layerDepth;
    
        // interpolation of texture coordinates
        float weight = afterDepth / (afterDepth - beforeDepth);
        float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

        return finalTexCoords;
    }
    
    return currentTexCoords;
}

fragment float4 ParallaxMap_ParallaxMap_fragment(VertexOut in [[stage_in]],
                                                constant UniformBlock& ubo [[buffer(0)]],
                                                texture2d<float> diffuseMap [[texture(0)]],
                                                texture2d<float> normalMap [[texture(1)]],
                                                texture2d<float> depthMap [[texture(2)]],
                                                sampler diffuseSampler [[sampler(0)]],
                                                sampler normalSampler [[sampler(1)]],
                                                sampler depthSampler [[sampler(2)]]) {
    // offset texture coordinates with Parallax Mapping
    float3 viewDir = normalize(in.TangentViewPos - in.TangentFragPos);
    float2 texCoords = in.TexCoords;
    if(ubo.floatPool[23] > 0.5){
        if(ubo.floatPool[41] > 0.5){
            texCoords = ParallaxMappingSteep(in.TexCoords, viewDir, depthMap, depthSampler, ubo);       
        }else{
            texCoords = ParallaxMapping(in.TexCoords, viewDir, depthMap, depthSampler, ubo);       
        }
        
        if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0)
            discard_fragment();
    }
    
    // obtain normal from normal map
    float3 normal = normalMap.sample(normalSampler, texCoords).rgb;
    normal = normalize(normal * 2.0 - 1.0);   
   
    // get diffuse color
    float3 color = diffuseMap.sample(diffuseSampler, texCoords).rgb;
    // ambient
    float3 ambient = 0.1 * color;
    // diffuse
    float3 lightDir = normalize(in.TangentLightPos - in.TangentFragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * color;
    // specular    
    float3 reflectDir = reflect(-lightDir, normal);
    float3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    float3 specular = float3(0.2) * spec;
    return float4(ambient + diffuse + specular, 1.0);
}