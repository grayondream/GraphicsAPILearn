#include <metal_stdlib>
using namespace metal;
// __MAT_HELPERS__ (auto-added: MSL lacks inverse()/mat4(mat3))
float3x3 mat3Inverse(float3x3 m) {
    float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2];
    float a10 = m[1][0], a11 = m[1][1], a12 = m[1][2];
    float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2];
    float b01 =  a22*a11 - a12*a21;
    float b11 = -a22*a10 + a12*a20;
    float b21 =  a21*a10 - a11*a20;
    float det = a00*b01 + a01*b11 + a02*b21;
    float id = 1.0 / det;
    return float3x3(
        b01*id, (-a22*a01 + a02*a21)*id, ( a12*a01 - a02*a11)*id,
        b11*id, ( a22*a00 - a02*a20)*id, (-a12*a00 + a02*a10)*id,
        b21*id, (-a21*a00 + a01*a20)*id, ( a11*a00 - a01*a10)*id
    );
}
float4x4 mat4FromMat3(float3x3 m) {
    return float4x4(float4(m[0], 0.0), float4(m[1], 0.0), float4(m[2], 0.0), float4(0.0, 0.0, 0.0, 1.0));
}

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
                                     constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.FragPos = float3(ubo.model * float4(in.pos, 1.0));   
    out.TexCoords = in.textureCoord;
    
    float3 n = ubo.floatPool[34] > 0.5 ? -in.normal : in.normal;   // inverse_normals
    
    float3x3 normalMatrix = transpose(mat3Inverse(float3x3(ubo.model[0].xyz, ubo.model[1].xyz, ubo.model[2].xyz)));
    out.Normal = normalize(normalMatrix * n);
    
    out.position = ubo.projection * ubo.view * ubo.model * float4(in.pos, 1.0);
    return out;
}

fragment float4 Hdr_Lighting_fragment(VertexOut in [[stage_in]],
                                      constant UniformBlock& ubo [[buffer(8)]],
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