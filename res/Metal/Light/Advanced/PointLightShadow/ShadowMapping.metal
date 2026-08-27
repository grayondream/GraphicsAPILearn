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

vertex VertexOut PointLightShadow_ShadowMapping_vertex(VertexIn in [[stage_in]],
                                                      constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.FragPos = float3(ubo.model * float4(in.pos));
    if(ubo.floatPool[34] > 0.5) // reverse_normals：a slight hack to make sure the outer large cube displays lighting from the 'inside' instead of the default 'outside'.
        out.Normal = transpose(inverse(float3x3(ubo.model))) * (-1.0 * in.normal.xyz);
    else
        out.Normal = transpose(inverse(float3x3(ubo.model))) * in.normal.xyz;
    out.TexCoords = in.textureCoord;
    out.position = ubo.projection * ubo.view * ubo.model * float4(in.pos);
    return out;
}

float3 gridSamplingDisk[20] = float3[]
(
   float3(1, 1,  1), float3( 1, -1,  1), float3(-1, -1,  1), float3(-1, 1,  1), 
   float3(1, 1, -1), float3( 1, -1, -1), float3(-1, -1, -1), float3(-1, 1, -1),
   float3(1, 1,  0), float3( 1, -1,  0), float3(-1, -1,  0), float3(-1, 1,  0),
   float3(1, 0,  1), float3(-1,  0,  1), float3( 1,  0, -1), float3(-1, 0, -1),
   float3(0, 1,  1), float3( 0, -1,  1), float3( 0, -1, -1), float3( 0, 1, -1)
);

float ShadowCalculation(float3 fragPos, constant UniformBlock& ubo, depthcube<float> depthMap) {
    // get vector between fragment position and light position
    float3 fragToLight = fragPos - ubo.vec4Pool[2].xyz;   // lightPos
    // ise the fragment to light vector to sample from the depth map    
    float closestDepth = depthMap.sample(depthSampler, fragToLight).r;
    // it is currently in linear range between [0,1], let's re-transform it back to original depth value
    closestDepth *= ubo.floatPool[17];   // far_plane
    // now get current linear depth as the length between the fragment and light position
    float currentDepth = length(fragToLight);
    // test for shadows
    float bias = 0.05; // we use a much larger bias since depth is now in [near_plane, far_plane] range
    float shadow = currentDepth -  bias > closestDepth ? 1.0 : 0.0;        
    // display closestDepth as debug (to visualize depth cubemap)
    // FragColor = vec4(vec3(closestDepth / far_plane), 1.0);    
        
    return shadow;
}

float ShadowCalculationWithPCF(float3 fragPos, constant UniformBlock& ubo, depthcube<float> depthMap, sampler depthSampler) {
    // get vector between fragment position and light position
    float3 fragToLight = fragPos - ubo.vec4Pool[2].xyz;   // lightPos
    // use the fragment to light vector to sample from the depth map    
    // float closestDepth = texture(depthMap, fragToLight).r;
    // it is currently in linear range between [0,1], let's re-transform it back to original depth value
    // closestDepth *= far_plane;
    // now get current linear depth as the length between the fragment and light position
    float currentDepth = length(fragToLight);
    // test for shadows
    // float bias = 0.05; // we use a much larger bias since depth is now in [near_plane, far_plane] range
    // float shadow = currentDepth -  bias > closestDepth ? 1.0 : 0.0;
    // PCF
    // float shadow = 0.0;
    // float bias = 0.05; 
    // float samples = 4.0;
    // float offset = 0.1;
    // for(float x = -offset; x < offset; x += offset / (samples * 0.5))
    // {
        // for(float y = -offset; y < offset; y += offset / (samples * 0.5))
        // {
            // for(float z = -offset; z < offset; z += offset / (samples * 0.5))
            // {
                // float closestDepth = texture(depthMap, fragToLight + float3(x, y, z)).r; // use lightdir to lookup cubemap
                // closestDepth *= far_plane;   // Undo mapping [0;1]
                // if(currentDepth - bias > closestDepth)
                    // shadow += 1.0;
            // }
        // }
    // }
    // shadow /= (samples * samples * samples);
    float shadow = 0.0;
    float bias = 0.15;
    int samples = 20;
    float viewDistance = length(ubo.vec4Pool[0].xyz - fragPos);   // viewPos
    float diskRadius = (1.0 + (viewDistance / ubo.floatPool[17])) / 25.0;   // far_plane
    for(int i = 0; i < samples; ++i)
    {
        float closestDepth = depthMap.sample(depthSampler, fragToLight + gridSamplingDisk[i] * diskRadius).r;
        closestDepth *= ubo.floatPool[17];   // far_plane, undo mapping [0;1]
        if(currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);
        
    // display closestDepth as debug (to visualize depth cubemap)
    // FragColor = vec4(vec3(closestDepth / far_plane), 1.0);    
        
    return shadow;
}

fragment float4 PointLightShadow_ShadowMapping_fragment(VertexOut in [[stage_in]],
                                                        constant UniformBlock& ubo [[buffer(0)]],
                                                        texture2d<float> diffuseTexture [[texture(0)]],
                                                        depthcube<float> depthMap [[texture(1)]],
                                                        sampler diffuseSampler [[sampler(0)]],
                                                        sampler depthSampler [[sampler(1)]]) {
    float3 color = diffuseTexture.sample(diffuseSampler, in.TexCoords).rgb;
    float3 normal = normalize(in.Normal);
    float3 lightColor = float3(1.);
    // ambient
    float3 ambient = 0.3 * lightColor;
    // diffuse
    float3 lightDir = normalize(ubo.vec4Pool[2].xyz - in.FragPos);   // lightPos
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * lightColor;
    // specular
    float3 viewDir = normalize(ubo.vec4Pool[0].xyz - in.FragPos);   // viewPos
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = 0.0;
    float3 halfwayDir = normalize(lightDir + viewDir);  
    spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    float3 specular = spec * lightColor;    
    // calculate shadow
    float shadow = 0.0;
    if(ubo.floatPool[19] > 0.5){   // shadows
        if(ubo.floatPool[40] > 0.5){   // enablePCF
            shadow = ShadowCalculationWithPCF(in.FragPos, ubo, depthMap, depthSampler);
        }else{
            shadow = ShadowCalculation(in.FragPos, ubo, depthMap);
        }
    }
     
    float3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;    
    float4 FragColor;
    if(ubo.floatPool[45] > 0.5){   // light
        FragColor = float4(lightColor, 1.0);
    }else{
        FragColor = float4(lighting, 1.0);
    }
    
    //FragColor = vec4(float3(shadow), 1.0);
    return FragColor;
}