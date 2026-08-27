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

vertex VertexOut BlinnPhong_Object_vertex(VertexIn in [[stage_in]],
                                          constant UniformBlock& ubo [[buffer(0)]]) {
    VertexOut out;
    out.FragPos = in.pos.xyz;
    out.Normal = in.normal.xyz;
    out.TexCoords = in.textureCoord;
    out.position = ubo.projection * ubo.view * in.pos;
    return out;
}

fragment float4 BlinnPhong_Object_fragment(VertexOut in [[stage_in]],
                                           constant UniformBlock& ubo [[buffer(0)]],
                                           texture2d<float> textureSampler [[texture(0)]],
                                           sampler sampler2D [[sampler(0)]]) {
    float3 color = textureSampler.sample(sampler2D, in.TexCoords).rgb;
    // ambient
    float3 ambient = 0.05 * color;
    // diffuse
    float3 lightDir = normalize(ubo.lights[0].position.xyz - in.FragPos);
    float3 normal = normalize(in.Normal);
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * color;
    // specular
    float3 viewDir = normalize(ubo.vec4Pool[0].xyz - in.FragPos);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = 0.0;
    if(ubo.floatPool[21] > 0.5){
        float3 halfwayDir = normalize(lightDir + viewDir);  
        spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    }else{
        float3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);
    }
    float3 specular = float3(0.3) * spec; // assuming bright white light color
    return float4(ambient + diffuse + specular, 1.0);
}